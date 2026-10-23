Closes: https://github.com/ClickHouse/ClickHouse/issues/127129
Related: https://github.com/ClickHouse/ClickHouse/issues/115694
Related: https://github.com/ClickHouse/ClickHouse/pull/115696

Permission changes can repeatedly rebuild the same effective access rights for distinct access contexts of a user, even when those contexts use the same enabled roles. With large per-table grant sets, this repeated work can dominate `GRANT` and `REVOKE` latency. Different current databases are one way to create such contexts; identical context parameters can already share the existing context cache.

`EffectiveAccessRightsCache` allows these contexts to reuse immutable results when the user and enabled-role snapshots and implicit-expansion settings match. Entries use weak references and bounded incremental expiry checks. Contexts with authentication-method `GRANTS` restrictions do not participate in sharing, and permission-change notifications remain synchronous.

Local Linux ARM64/Docker benchmarks compare parent `dd7889a115c63e16f3bcbc9f843e4633fb2fe1e0` (A) with `3319c0bf1944237c0ed87eac0fb0affc61a7d609` (B), using a synthetic fixture with 75,123 direct privilege rows across 15,580 table paths plus role grants. Each container has four vCPUs and 6 GiB; runs use A/B/B/A order and 20 measured samples per operation and variant after warm-up.

The cases below use the same user and enabled roles with different current databases. Session counts are not exact access-context counts: HTTP also retains a default-database context.

| Named sessions | Operation | A median, ms | B median, ms | Speedup |
|---|---|---:|---:|---:|
| 10 | `GRANT` | 2068.202 | 108.179 | 19.12x |
| 50 | `GRANT` | 7295.179 | 107.898 | 67.61x |
| 10 | `REVOKE` | 2143.623 | 108.403 | 19.77x |
| 50 | `REVOKE` | 7398.014 | 107.452 | 68.85x |

Controls already sharing an access context show no consistent benefit. These measurements cover local direct-user permission changes with synthetic ACLs; they exclude `ON CLUSTER`, Keeper and distributed DDL delays.

The measured trade-off is about 5–7% median overhead with 256–4096 distinct role combinations and no reuse. A separate synthetic maintenance workload preloads 512 users × 32 combinations and times four readers and one writer. In the 16-case repeat, reader P99 was 1.461/1.632 ms and writer P99 was 1.201/1.385 ms (A/B), increases of 0.171/0.184 ms (about 12%/15%). An earlier run recorded roughly 2x P99. In a separately instrumented slow block, cache-lock waiting and holding accounted for a median 0.21% of request wall time among writers taking over 1 ms; shared `DiskAccessStorage` lock contention was also observed blocking requests. These diagnostics identify a source of slow requests outside the cache, while the repeated A/B measurements quantify the end-to-end cost in this workload.


Full results, raw samples and reproduction instructions: [benchmark evidence](https://github.com/a819721810/ClickHouse/blob/9834df01a7a957b0513d61a113637301f15c6ada/benchmark-evidence/pr117270/2026-10-23/benchmark.md). Tail-latency diagnostics and limitations: [investigation](https://github.com/a819721810/ClickHouse/blob/9834df01a7a957b0513d61a113637301f15c6ada/benchmark-evidence/pr117270/2026-10-23/tail-investigation.md).

### Changelog category (leave one):
- Performance Improvement

### Changelog entry (a [user-readable short description](https://github.com/ClickHouse/ClickHouse/blob/master/docs/changelog_entry_guidelines.md) of the changes that goes into CHANGELOG.md):
Speed up `GRANT` and `REVOKE` for users with large grant sets when sessions can share calculated access rights.

