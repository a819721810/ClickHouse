# PR #117270 evidence, 2026-10-23

Related: https://github.com/ClickHouse/ClickHouse/pull/117270

The code revision prepared for publication is `6749388647e0b659f18561a172a0eb19cd2db20f`. See [benchmark.md](benchmark.md) for local A/B results, [tail-investigation.md](tail-investigation.md) for the later tail investigation, and the CSV/JSON files for summaries. The measured full-server B is `3319c0bf1944237c0ed87eac0fb0affc61a7d609`; later follow-up commits and any merge with `master` are not a newly measured binary.

The two archives preserve the checked local evidence bundles, including measured harnesses, reproduction instructions, raw observations, source identities and checksums. They exclude multi-gigabyte server binaries; rebuilding/re-running requires the exact inputs specified by each archive. Some stored paths refer to the original machine and must be adjusted after relocation. These are evidence bundles, not standalone container images. No performance-CI or production result is claimed.

The evidence lives only on this fork's evidence branch; these reports and archives are not added to the upstream PR's source diff. The source PR continues to target `master` directly.
