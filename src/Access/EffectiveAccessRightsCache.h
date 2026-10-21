#pragma once

#include <Core/UUID.h>
#include <Access/ImplicitExpansionSettings.h>
#include <base/defines.h>
#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <unordered_map>


namespace DB
{

struct User;
using UserPtr = std::shared_ptr<const User>;
struct EnabledRolesInfo;
class AccessRights;

/// Caches the effective access rights of a user with their enabled roles
/// (i.e. what ContextAccess::calculateAccessRights computes) so that all sessions
/// of the same user share one calculation instead of doing one per session.
///
/// A hit requires the exact immutable `User` and `EnabledRolesInfo` snapshots and the
/// settings the implicit expansion depends on. Any user or role change produces new
/// snapshots, so no explicit invalidation is needed.
///
/// Entries hold only weak references to the results, like the other *Cache classes in this
/// directory: results can only be reused while some context still holds them. Dead
/// entries are removed incrementally without retaining their privilege trees.
class EffectiveAccessRightsCache
{
public:
    struct Result
    {
        std::shared_ptr<const AccessRights> access;
        std::shared_ptr<const AccessRights> access_with_implicit;
    };

    /// Returns the result previously stored for exactly these snapshots and settings, or nullopt.
    std::optional<Result> find(const UUID & user_id, const UserPtr & user,
        const std::shared_ptr<const EnabledRolesInfo> & roles_info,
        const ImplicitExpansionSettings & settings);

    /// Stores the result calculated from exactly these snapshots and settings.
    void store(const UUID & user_id, const UserPtr & user,
        const std::shared_ptr<const EnabledRolesInfo> & roles_info,
        const ImplicitExpansionSettings & settings,
        const std::shared_ptr<const AccessRights> & access,
        const std::shared_ptr<const AccessRights> & access_with_implicit);

private:
    struct Key
    {
        UUID user_id;
        const EnabledRolesInfo * roles_info;
        ImplicitExpansionSettings settings;

        bool operator ==(const Key &) const = default;
    };

    struct KeyHash
    {
        size_t operator()(const Key & key) const;
    };

    struct Entry
    {
        Key key;

        /// Keep one user version per role/settings combination, reusing the allocated
        /// entry when a user update refreshes all its contexts. A different `User`
        /// snapshot is always a miss, even if another context still uses the old one.
        const User * user_address;

        /// Validate the weak snapshots as well as their raw addresses, so an
        /// allocator reusing the address of an expired snapshot cannot produce a hit.
        std::weak_ptr<const User> user;
        std::weak_ptr<const EnabledRolesInfo> roles_info;

        std::weak_ptr<const AccessRights> access;
        std::weak_ptr<const AccessRights> access_with_implicit;
    };

    using Entries = std::list<Entry>;

    bool isExpired(const Entry & entry) const TSA_REQUIRES(mutex);
    void eraseEntry(Entries::iterator entry) TSA_REQUIRES(mutex);

    /// Examine a bounded number of entries per `store`. List iterators remain valid when
    /// the hash index rehashes, and the cursor advances across both live and dead entries.
    void sweepExpiredEntries() TSA_REQUIRES(mutex);

    static constexpr size_t SWEEP_BUDGET = 2;

    std::mutex mutex;
    Entries entries TSA_GUARDED_BY(mutex);
    std::unordered_map<Key, Entries::iterator, KeyHash> index TSA_GUARDED_BY(mutex);
    Entries::iterator next_entry_to_sweep TSA_GUARDED_BY(mutex) = entries.end();
};

}
