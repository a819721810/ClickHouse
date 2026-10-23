# Concurrent maintenance: short summary

A synthetic workload preloaded 512 users × 32 role combinations and ran four readers alongside one permission-update writer. The 16-case repeat measured:

| Request | Before P99, ms | After P99, ms | Increase |
|---|---:|---:|---:|
| Reader | 1.461 | 1.632 | **0.171 ms / 12%** |
| Writer | 1.201 | 1.385 | **0.184 ms / 15%** |

An earlier run recorded roughly 2× P99. Both versions produced slow blocks. Separate instrumentation found most slow-writer time outside the cache critical sections and observed shared access-storage lock contention; it did not attribute the entire A/B difference.

These local ARM64/Docker results describe this maintenance workload. The [benchmark summary](benchmark.md) covers the large-grant-set speedup and the 5–7% overhead when results cannot be reused.

[Full original investigation and diagnostic evidence](https://github.com/a819721810/ClickHouse/blob/9834df01a7a957b0513d61a113637301f15c6ada/benchmark-evidence/pr117270/2026-10-23/tail-investigation.md).
