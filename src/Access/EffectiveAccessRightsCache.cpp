#include <Access/EffectiveAccessRightsCache.h>
#include <boost/container_hash/hash.hpp>
#include <algorithm>


namespace DB
{

size_t EffectiveAccessRightsCache::KeyHash::operator()(const Key & key) const
{
    size_t hash = std::hash<UUID>{}(key.user_id);
    boost::hash_combine(hash, key.roles_info);
    boost::hash_combine(hash, key.settings.select_from_system_db_requires_grant);
    boost::hash_combine(hash, key.settings.select_from_information_schema_requires_grant);
    boost::hash_combine(hash, key.settings.user_query_log_enabled);
    boost::hash_combine(hash, key.settings.table_engines_require_grant);
    return hash;
}


bool EffectiveAccessRightsCache::isExpired(const Entry & entry) const
{
    return entry.user.expired() || entry.roles_info.expired()
        || entry.access.expired() || entry.access_with_implicit.expired();
}


std::optional<EffectiveAccessRightsCache::Result> EffectiveAccessRightsCache::find(
    const UUID & user_id,
    const UserPtr & user,
    const std::shared_ptr<const EnabledRolesInfo> & roles_info,
    const ImplicitExpansionSettings & settings)
{
    std::lock_guard lock{mutex};
    auto it = index.find(Key{user_id, roles_info.get(), settings});
    if (it == index.end())
        return std::nullopt;

    const Entry & entry = *it->second;
    if (entry.user_address != user.get())
        return std::nullopt;

    auto stored_user = entry.user.lock();
    auto stored_roles_info = entry.roles_info.lock();
    auto stored_access = entry.access.lock();
    auto stored_access_with_implicit = entry.access_with_implicit.lock();

    if ((stored_user == user) && (stored_roles_info == roles_info) && stored_access && stored_access_with_implicit)
        return Result{std::move(stored_access), std::move(stored_access_with_implicit)};

    return std::nullopt;
}


void EffectiveAccessRightsCache::store(
    const UUID & user_id,
    const UserPtr & user,
    const std::shared_ptr<const EnabledRolesInfo> & roles_info,
    const ImplicitExpansionSettings & settings,
    const std::shared_ptr<const AccessRights> & access,
    const std::shared_ptr<const AccessRights> & access_with_implicit)
{
    std::lock_guard lock{mutex};

    Key key{user_id, roles_info.get(), settings};
    auto it = index.find(key);
    if (it == index.end())
    {
        /// Allocate locally first so an exception leaves both containers unchanged.
        /// `splice` preserves the stored iterator and does not allocate or throw.
        Entries new_entry;
        new_entry.push_back(Entry{key, user.get(), user, roles_info, access, access_with_implicit});
        index.emplace(key, new_entry.begin());
        entries.splice(entries.end(), new_entry);
    }
    else
    {
        Entry & entry = *it->second;
        entry.user_address = user.get();
        entry.user = user;
        entry.roles_info = roles_info;
        entry.access = access;
        entry.access_with_implicit = access_with_implicit;
    }

    sweepExpiredEntries();
}


void EffectiveAccessRightsCache::eraseEntry(Entries::iterator entry)
{
    index.erase(entry->key);
    entries.erase(entry);
}


void EffectiveAccessRightsCache::sweepExpiredEntries()
{
    for (size_t remaining = std::min(SWEEP_BUDGET, entries.size()); remaining; --remaining)
    {
        if (next_entry_to_sweep == entries.end())
            next_entry_to_sweep = entries.begin();

        auto entry = next_entry_to_sweep++;
        if (isExpired(*entry))
            eraseEntry(entry);
    }
}

}
