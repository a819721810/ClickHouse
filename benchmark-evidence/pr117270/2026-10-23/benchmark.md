# Effective access rights cache: benchmark summary

**With large grant sets, median local `GRANT`/`REVOKE` latency fell from about 7.3 seconds to 108 milliseconds across 50 sessions using different current databases — about 68× faster.**

## When it speeds up

Distinct contexts of the same user with the same enabled roles can reuse calculated access rights. This fixture had 75,123 direct privilege rows across 15,580 table paths, plus role grants; sessions selected different current databases.

| Named sessions | Operation | Before, median ms | After, median ms | Speedup |
|---|---|---:|---:|---:|
| 10 | `GRANT` | 2068.202 | 108.179 | **19.12×** |
| 10 | `REVOKE` | 2143.623 | 108.403 | **19.77×** |
| 50 | `GRANT` | 7295.179 | 107.898 | **67.61×** |
| 50 | `REVOKE` | 7398.014 | 107.452 | **68.85×** |

## When it adds little benefit or some overhead

- **Already-shared contexts:** no consistent improvement; the existing cache already avoids this repeated work.
- **Different role combinations with no reuse:** 256–4096 combinations measured **5–7% higher median latency**.
- **Concurrent maintenance:** a separate four-reader/one-writer repeat measured reader/writer P99 increases of **0.171/0.184 ms (12%/15%)**. See the [short tail-latency summary](tail-investigation.md).

## Measurement scope and evidence

Local ARM64/Docker, four vCPUs and 6 GiB per container; synthetic workloads. The main comparison used A/B/B/A order and 20 measured samples per operation and variant. It measures local permission updates, excluding distributed `ON CLUSTER` delays.

Measured versions: parent `dd7889a115c6` and optimized `3319c0bf194`. [Full original report and reproduction details](https://github.com/a819721810/ClickHouse/blob/9834df01a7a957b0513d61a113637301f15c6ada/benchmark-evidence/pr117270/2026-10-23/benchmark.md) · [Raw samples](benchmark-samples.csv).
