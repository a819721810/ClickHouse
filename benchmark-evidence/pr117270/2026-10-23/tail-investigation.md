# Concurrent maintenance tail-latency investigation, 2026-10-23

Verdict: unstable short test / local-only evidence. A reproduced millisecond-scale slow block was not dominated by the new cache's critical sections. The unique trigger of the original historical adverse block cannot be reconstructed because it lacked per-request lock and stack records. This does not establish zero overhead: the uninstrumented repeat below remains adverse to B.

The fixture preloads 512 users with 32 role combinations each (about 16,384 retained access contexts). Timed concurrency is four reader requests plus one unrelated permission-update writer, not 512 concurrent requests. Each case has 2,048 synchronized rounds: 8,192 readers and 2,048 writers. Each container has four vCPUs on CPU set 0–3 and 6 GiB; Python and the server share those resources. A and B are the exact commits specified in `benchmark.md`. Different diagnostic settings are never pooled into a single A/B result.

## Uninstrumented original-script repeat

Four A/B/B/A groups, eight cases per variant. Original `stress_server.py` and `run_case.py` are byte-identical to the preceding benchmark scripts; there is no segmented HTTP timing instrumentation in this phase. The manifest's inherited scope string mentions instrumentation, but the actual script and execution path do not use it.

| Request | Variant | Median / P95 / P99, ms |
|---|---|---:|
| reader | A | 0.469 / 0.887 / 1.461 |
| reader | B | 0.498 / 1.208 / 1.632 |
| writer | A | 0.349 / 0.723 / 1.201 |
| writer | B | 0.393 / 0.992 / 1.385 |

Pooled B/A P99 is 1.117x for readers and 1.154x for writers. Both variants show reader blocks around 1.8–2.1 ms P99. B's median and P95 are also worse; the remaining difference is not fully explained and must not be dismissed as noise. The original roughly 2x observation is retained in the earlier archive.

## Direct lock timing

Four separate instrumented B cases record exactly 10,240 cache `find` and 2,048 `store` lock acquisitions per timed window, with no overflow. In the slow case, reader/writer P99 is 2.157/2.092 ms. Across its 78 writers exceeding 1 ms, the median sum of each writer's own cache-lock waiting and holding is 2.938 microseconds, or a median 0.21% of request wall time. The slowest writer takes 7.598 ms; its cache critical sections total 1.709 microseconds, and approximately 7.499 ms precedes its first cache entry. Pairing uses writer TID and iteration, and verifies both complete lock intervals fall inside the writer request window. Instrumentation perturbs execution; these cases support attribution, not A/B speedup claims.

The shared `DiskAccessStorage` mutex provides a separate cross-user blocking path. A captured update holds it for 2,046 microseconds while two other threads wait 2,058/2,036 microseconds; all five requests in that round take about 2.3–2.5 ms. `DiskWriteElapsedMicroseconds` for that writer is only 3 microseconds, so the whole hold must not be labeled a file write or `fsync`. Metadata/rename and scheduling may contribute; the update path has no explicit `fsync`. Baseline A also produces reader/writer P99 2.007/1.512 ms with the access directory verified on tmpfs, so neither the new cache nor overlay access-file writes are necessary for this type of tail.

## Within-run controls

Two-round alternating assignments balance `GRANT` and `REVOKE` in each state. Disabling parallel output formatting reduces reader median about 20% on both variants, but P99 only about 4%; it does not similarly improve writer P99. This confirms common overhead, not a complete explanation for 2x.

Combining HTTP headers and body into one `HTTPConnection.send` call, instead of the standard two calls, reduces A writer P99 from 0.752 to 0.578 ms and B from 0.672 to 0.575 ms. Reader tails also improve. Every measured request verifies the call count; TCP packets and kernel syscall counts were not measured. This is evidence of client/HTTP interaction overhead, not proof of the original unique trigger.

GC overlaps only 2 of 424 requests exceeding 1 ms in a separate slow block. Slow blocks also occur with a multi-process generator, separated client/server CPUs, and a longer query-log flush interval. None of GC, a single Python GIL, shared CPU affinity, or the original query-log interval alone explains the phenomenon. CPU quota counters show no throttling where captured; guest/hypervisor scheduling remains possible. The host was not exclusive.

## Sampling and provenance limits

Separate 1 ms profiling produces 578 CPU and 22,731 Real samples. Parallel formatting helpers, HTTP body socket polling, and access-storage paths appear in waiting stacks. CPU samples are sparse; roughly half of Real leaf frames are unresolved, although parent waiting chains are readable. Inclusive helper-thread Real samples cannot be converted into a single request's wall-time fraction. A few merge `ABORTED` messages occur after evidence export during server shutdown, without failed measured requests.

All 76 accepted diagnostic cases and 276 isolation audits are indexed by `tail-investigation-summary.json`. There are 152 case-operation summary rows. Failed, interrupted, or inactive-lock-instrumentation attempts are recorded separately and excluded from corresponding conclusions. Native lock attribution uses the exact ELF and verified call sites; different binaries require regenerated symbol ranges.

The evidence does not justify another cache rewrite solely because of the original 2x block, nor a universal non-regression claim. Longer interleaved end-to-end runs and separate accounting for computation, persistence, and request handling remain useful. Existing same-role/large-ACL speedup evidence is separate and is not extrapolated to production `ON CLUSTER`.
