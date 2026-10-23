# Indexed effective access rights cache: local benchmark evidence

Published on 2026-10-23. These are local measurements, not CI approval or production results.
The report below measures B at `3319c0bf1944237c0ed87eac0fb0affc61a7d609`, against exact-parent A at `dd7889a115c63e16f3bcbc9f843e4633fb2fe1e0`.
Subsequent commits extend the regression test (`39c1dcd1693`) and clarify a settings comment/remove an unreachable cursor adjustment (`6749388647e`). They were not rebuilt into the measured full-server binary. The latter received a separate ASan/UBSan component check; this is not full-server sanitizer coverage.

The original 512-user adverse block remains in the historical table below. Its subsequent investigation is reported in [tail-investigation.md](tail-investigation.md); it does not erase the original observation or prove zero regression. In particular, the later uninstrumented 16-case repeat still measures B's reader/writer P99 about 12%/15% above A. The zero-reuse 5–7% median overhead also remains.

## Local validation of indexed effective access rights cache

Related: https://github.com/ClickHouse/ClickHouse/pull/117270

A is the exact parent `dd7889a115c63e16f3bcbc9f843e4633fb2fe1e0` (official ARM Release artifact). B in this report is the optimized local commit `3319c0bf1944237c0ed87eac0fb0affc61a7d609`. The previous PR binary `de8c62d3fe99ad722f89346ef6a9e5cb77531278` is historical evidence only and is not pooled into the fresh A/B samples.

The implementation replaces per-user vector scans with a hash index over user UUID, the exact immutable `EnabledRolesInfo` snapshot, and all four `ImplicitExpansionSettings` values. Each slot keeps one user snapshot, checked exactly before a hit; user snapshot changes reuse the allocated slot; overlapping versions may miss but cannot share mismatched results. Each `store` examines at most two entries for expiry using a stable list cursor. Snapshot weak references are still validated on a hit, including address reuse. Average index lookup and insertion are constant time; normal hash-table rehashing can still take linear time.

### Method

- Native Linux ARM64 in Docker Desktop on Apple M5 Max, 18 host CPUs / 128 GiB RAM; Docker VM 90 GiB. Each full-server test container has four vCPUs, cpuset `0-3`, 6 GiB RAM, and no external network.
- Fresh sequential A/B/B/A blocks; two warm-up and ten measured grant/revoke pairs per block, hence twenty measured samples per operation and variant. Client loopback wall time includes the complete local access change. Validation and setup are outside the timed section.
- No compilation overlaps measurements. Eleven unrelated containers stay paused and two stay stopped; state is audited before, after, and about every five seconds during cases. The macOS host is not claimed to be exclusive.
- Exact runtime identity is asserted through `system.build_options.GIT_HASH`. Clang/LLD 22.1.8, `RelWithDebInfo`, optimization, ThinLTO and reported feature options match after normalizing linker worker/cache and two compiler-rt archive path differences. Raw options remain in each result.
- These are local measurements, not performance-CI or production measurements. They exclude `ON CLUSTER`, Keeper and distributed DDL queue delays.

### Many distinct role combinations: fresh A/B

One user, small ACL, one persistent named HTTP session visits every subset of 5/8/10/12 actual roles. The existing context cache retains 32/256/1024/4096 distinct role combinations. Thus these are combination/context counts, not thousands of roles or TCP connections. Updating the user requires one calculation per combination; this intentionally removes same-role reuse and stresses cache overhead.

| Combinations | Operation | A median / P95 ms | Optimized B median / P95 ms | B / A |
|---:|---|---:|---:|---:|
| 32 | `GRANT` | 0.866 / 0.954 | 0.835 / 0.878 | 0.964x |
| 32 | `REVOKE` | 0.808 / 1.106 | 0.788 / 0.848 | 0.975x |
| 256 | `GRANT` | 5.566 / 6.364 | 5.951 / 6.937 | 1.069x |
| 256 | `REVOKE` | 5.180 / 5.862 | 5.551 / 7.240 | 1.072x |
| 1024 | `GRANT` | 21.094 / 22.690 | 22.139 / 22.927 | 1.050x |
| 1024 | `REVOKE` | 19.718 / 20.583 | 20.925 / 21.753 | 1.061x |
| 4096 | `GRANT` | 84.929 / 90.949 | 89.506 / 100.097 | 1.054x |
| 4096 | `REVOKE` | 79.640 / 101.816 | 83.882 / 99.882 | 1.053x |

The current repeat still measures 5.0% to 7.2% median overhead for 256–4096 zero-reuse combinations. The quadratic scan penalty is removed, but the result is not zero overhead or a universal non-regression. This repeat takes precedence over the smaller differences in the earlier replication.

Historical comparison, kept separate: before this change, the old PR measured 45.630 / 44.699 ms for 1024 combinations and 554.034 / 542.299 ms for 4096 combinations (`GRANT` / `REVOKE` medians), versus its contemporaneous parent 24.018 / 22.756 ms and 91.781 / 85.669 ms. The fresh table above is the evidence for the optimized version; historical numbers are not a paired B-versus-old-PR experiment.

An earlier accepted replication of this same final binary on 2026-10-21 had median B/A ratios from 0.989 to 1.019, but its 4096-combination `REVOKE` P95 was 111.830 ms versus A 83.867 ms. The slow block also used more CPU; its cause was not identified. Thus removal of the quadratic median slowdown is not a claim that every tail percentile improves. The earlier replication is retained separately in `previous_accepted_runs.json`; it is not pooled with the primary table.

### Production-scale synthetic ACL: fresh A/B

Identical synthetic fixture in all 24 runs: 75,123 direct privilege rows across 15,580 table paths, plus one enabled role with 471 rows across 465 paths. It matches recorded aggregate scale, not the exact production topology; it contains no column grants, partial revokes or grant-option distribution. `shared` uses identical context parameters; `distinct` changes each named session default database while keeping effective roles identical. Session counts are not exact counts of retained `ContextAccess` objects.

| Context parameters | Sessions | Operation | A median / P95 ms | Optimized B median / P95 ms | A / B speedup |
|---|---:|---|---:|---:|---:|---:|
| shared | 1 | `GRANT` | 136.027 / 143.547 | 125.680 / 137.210 | 1.08x |
| shared | 1 | `REVOKE` | 134.260 / 151.665 | 126.345 / 131.647 | 1.06x |
| shared | 10 | `GRANT` | 135.959 / 157.257 | 128.313 / 139.423 | 1.06x |
| shared | 10 | `REVOKE` | 131.800 / 154.670 | 128.779 / 139.549 | 1.02x |
| shared | 50 | `GRANT` | 125.148 / 131.943 | 124.815 / 141.524 | 1.00x |
| shared | 50 | `REVOKE` | 123.540 / 134.255 | 124.363 / 137.149 | 0.99x |
| distinct | 1 | `GRANT` | 240.135 / 249.154 | 110.440 / 127.989 | 2.17x |
| distinct | 1 | `REVOKE` | 238.089 / 245.834 | 110.112 / 124.938 | 2.16x |
| distinct | 10 | `GRANT` | 2068.202 / 2405.922 | 108.179 / 115.310 | 19.12x |
| distinct | 10 | `REVOKE` | 2143.623 / 2305.859 | 108.403 / 111.430 | 19.77x |
| distinct | 50 | `GRANT` | 7295.179 / 9038.295 | 107.898 / 125.418 | 67.61x |
| distinct | 50 | `REVOKE` | 7398.014 / 8567.440 | 107.452 / 118.073 | 68.85x |

Optimized B recorded one calculation and N hits in every distinct-context update (including N=1), and one calculation with zero hits in the shared controls. These events were introduced by the PR and do not exist on A; zero raw A event values must not be read as zero calculation work. All existing sessions check the changed permission and continue to reject unrelated writes after every operation. Already-shared contexts are a control and should not be advertised as a general connection-count speedup.

### Correctness and concurrent maintenance checks

- Unmodified `05054_effective_access_rights_cache.sh` and its reference pass on the optimized full server, including both cache-hit assertions.
- New `05055_effective_access_rights_role_permissions.sh` passes: four established sessions with A/A/B/A+B roles are warmed first; direct role permission grants and revokes take effect in those sessions while `SHOW CREATE USER` stays unchanged. Both actual `SELECT` execution and `CHECK GRANT` are checked, with unrelated `INSERT` denied.
- Additional 1/10/50-session and mixed-role checks pass; authentication-method `GRANTS` isolation checks pass in full-first and restricted-first order.
- ASan/UBSan component checks on the committed cache implementation pass: all sixteen settings combinations, exact snapshot identity, reused object addresses, eventual release of 4096 retired entries, and 320,000 concurrent operations across identical and different live user snapshots. Payloads are simplified in this component check; it is not a sanitizer build of the full server.
- Multi-user maintenance matrix also uses A/B/B/A, 64 users x 16 combinations and 512 x 32 combinations, with four readers and 2048 unrelated writer updates per block. Reader success is checked by returned results; reader query logging is disabled. Writer completion is checked through `QueryFinish`. Short concurrent runs do not establish a universal P99 guarantee.

| Users x combinations | Requests | A median / P99 ms | Optimized B median / P99 ms |
|---|---|---:|---:|
| 64 x 16 | reader | 0.523 / 1.350 | 0.449 / 0.875 |
| 64 x 16 | writer | 0.400 / 1.107 | 0.351 / 0.795 |
| 512 x 32 | reader | 0.453 / 0.863 | 0.494 / 1.789 |
| 512 x 32 | writer | 0.332 / 0.743 | 0.389 / 1.439 |

The 512-user repeat includes a slow B block: pooled reader P99 is 2.07x A and writer P99 is 1.94x A. The cause is not identified. All functional checks and writer completions passed, but this observation prevents a claim of stable tail-latency improvement. Per-block results remain in the raw manifest and summary.


### Separate CPU diagnostic

A separate 4096-combination profile confirms that absolute CPU is dominated by permission-tree construction, implicit expansion and release. Visible cache frames account for 14/2047 CPU samples (0.68%) in B, but this is not an upper bound on all cache cost because inlining and lifecycle work may be charged elsewhere. It does not precisely explain the remaining 5–7% overhead or the concurrent pressure-tail observation.

Both variants requested a 100-microsecond sampling period; `Timer::set` clamps the effective period to at least one millisecond in both source versions. These diagnostic timings are excluded from the A/B tables. See `cpu-profile-analysis.md`, `cpu-profile-analysis.json` and the complete collapsed stacks for evidence and limitations.


All 213 container isolation audits passed. The interrupted `matrix_2e9ab2fab1` is excluded in full; the recovered monitor uses one daemon snapshot so normal removal of a disposable runner cannot break a separate inspection step. Per-suite UTC measurement windows and actual audit methods are recorded in `summary.json`; methods are not applied retroactively to earlier runs.

See `summary.json`, `benchmark-samples.csv`, raw manifests, per-case `result.json`, and `build_receipt.json` for block variation, CPU, counters, source identity and artifact hashes. No claim is made here about CI approval, universal tail-latency behavior, or production deployment.
