# Track C performance log

The native candidate uses a buffered CSV stream, a region-to-country array, and interned names with per-country membership bitsets. It counts decoded note bytes without allocating a string for every note. The reference uses Python's CSV reader, a dictionary join, and one Python string set per country. Both programs write and close the aggregate file before the wall timer stops.

Measured with Linux process wall time and peak RSS from wait4, using g++ 13.3.0, C++17 with -O3, and CPython 3.12.14. These are single runs on a shared development host. The sample reference ran first and the candidate second. The optimization choices above were measured together.

| Public workload | Reference wall | Candidate wall | Reference peak RSS | Candidate peak RSS |
|---|---:|---:|---:|---:|
| Sample seed 1, 332,000 records | 3.7069 s | 0.3393 s | 71,688 KiB | 17,068 KiB |
| Full seed 1, 3,320,000 records | Not timed | 1.7745 s | Not measured | 17,360 KiB |

Both public seed-1 input files matched the organizers' published hashes at each scale. The sample output matched the official Python result byte-for-byte. The full output matched the published SHA-256 directly. The two outputs contain 240 country aggregates each.

| Check | Output SHA-256 |
|---|---|
| Public sample seed 1 | `9035fed36823232dd41ae2d1b9d6d21aa7c73f49d8e95647e953a89d5ebe9a66` |
| Public full seed 1 | `d8088b1cd42ba1338b1345b107b55db7b96ef1f5066f3ff64fd26a8912a0f536` |
| Tiny seed 19, byte-identical to the reference | `6e4118916628e4b69adcb8a01b2539f5ca05632b310564d9b4cf2c1c7387a9e8` |

A separate boundary fixture matched the reference for embedded Unicode and BOM bytes, an escaped quote adjacent to a line feed, an empty name in quoted and unquoted form, and an unterminated final row. The official Dockerfile policy checker passed. The official container subsequently passed the public run below.

## First public hardware run

The [organizer’s practice board](https://speeddemon.dev/board/) scored commit `db8facdafaa0d3f4e570b75b835286da2fdad2ad` on the full public seed-1 input. Run `run-3fed99b2e442` passed the build, correctness, timeout, and sandbox gates.

| Wall time (s) | Peak memory (bytes) | Efficiency coefficient | Practice points |
| ---: | ---: | ---: | ---: |
| 1.185705 | 17457152 | 5.879511 | 38.467061 |

## First optimization pass

Read the CSV in blocks and carry incomplete records into the next block. Find field delimiters and escaped quotes in place. Intern names in a shared byte pool with exact comparisons on hash collisions. This preserves distinct-name counts across blocks and avoids allocating strings for every row. The candidate matched the full public output hash and the official Python oracle on three additional tiny datasets (seeds 47, 103 and 327).
