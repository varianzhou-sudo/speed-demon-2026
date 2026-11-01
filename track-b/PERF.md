# Track B local performance log

Measured native end-to-end elapsed times, including process startup and input/output, on the public generator's seed 1 sample (25,000 documents, 5,000 queries). Each implementation ran five times with alternating execution order. The elapsed time below is the mean of the middle three; memory is the median peak resident set size across five runs. The local machine was shared; Docker, the scoring host and official board were not used.

| Implementation | Elapsed (s) | Peak RSS (KiB) |
|---|---:|---:|
| Official Python reference | 3.1324 | 306,340 |
| Query-aware C++20 index | 0.4468 | 25,968 |

The C++ implementation retained postings only for query terms and used the rarest term to constrain phrase scans. It matched all 5,000 sample answers byte-for-byte. Sample result SHA-256: `eeeec1e9319da98a6f3c664c78f93e7078fd603c25c2bb3027174e81e3638a8f`.

A second run used tiny scale, seed 19, and matched the official reference for all 1,000 queries. SHA-256: `92ded523f7078667483fdcdbd2d9334a94a4df7645b4fac47c806132ce003bd6`.

The published Dockerfile policy checker accepted both pinned stages. A local static GCC 13.3 C++20 build reproduced the official sample checksum, the published worked example, and all six of the organizer's hand-calculated edge-case suites. The pinned GCC 15 container was not run locally.

Full public seed 1 also matched all three published SHA-256 digests (both input files and the result). The native program answered 50,000 queries on 250,000 documents and wrote output SHA-256 `f86973c210228141bcb6f1f1b10188ad2a9b65f71e7cd7cf4590b38de4e80927`. The single full run took 9.384 seconds on this shared machine, with peak RSS 382,240 KiB; no full-scale local reference timing was taken.

## First public hardware run

The [organizer’s practice board](https://speeddemon.dev/board/) scored commit `db8facdafaa0d3f4e570b75b835286da2fdad2ad` on the full public seed-1 input. Run `run-2f46b448665e` passed the build, correctness, timeout, and sandbox gates.

| Wall time (s) | Peak memory (bytes) | Efficiency coefficient | Practice points |
| ---: | ---: | ---: | ---: |
| 6.390913 | 394899456 | 3.782584 | 28.889432 |

## First optimization pass

Read the queries first, then index bounded groups of complete document lines in parallel. A query vocabulary limits the postings built for each shard. Each worker retains a bounded result summary per query. Merging these summaries in document order preserves the counts, ID sums and first hits without materializing a global result list. The candidate matched the full public output hash and the official Python oracle on three additional tiny datasets (seeds 47, 103 and 327).
