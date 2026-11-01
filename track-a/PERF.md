# Track A performance log

This local experiment replaces Python timestamp parsing with a fixed-width UTC-hour key. The C++17 implementation streams the input, stores unsigned 32-bit latencies, and sorts the values in each hour/endpoint group. Both versions write and close their output before the timer stops.

| Local workload | Official Python reference | C++17 candidate | Comparison |
|---|---:|---:|---|
| 310,000 rows, sample seed 1 | 5.821 s | 0.236 s | Median of three process wall times per implementation, run order alternated, warm filesystem cache |

Individual runs in seconds were 5.821, 3.507 and 8.543 for Python, and 0.181, 0.236 and 0.361 for C++. These times came from a shared Linux host. Both programs wrote 66,153 identical groups with SHA-256 `ca6311bbeb536fa55d85265bf6fd3d24cc9586f13de1ac73bd2a23ac3893aac3`, matching the published sample checksum.

On the full public seed 1, the implementation processed 3,100,000 input records and wrote 126,908 groups. The output matched the organizers' full SHA-256, `495ca09682b9673ef836a506496177680bb4a3af54255174befddb1f83481cd3`. An independent 1,000-row tiny dataset at seed 19 also matched the official Python reference byte for byte. Empty input produced the required empty output. The official Dockerfile policy check passed. A local static build used GCC 13.3.0; the official build and timing are recorded below.

## First public hardware run

The [organizer’s practice board](https://speeddemon.dev/board/) scored commit `db8facdafaa0d3f4e570b75b835286da2fdad2ad` on the full public seed-1 input. Run `run-53da8cf21dcf` passed the build, correctness, timeout, and sandbox gates.

| Wall time (s) | Peak memory (bytes) | Efficiency coefficient | Practice points |
| ---: | ---: | ---: | ---: |
| 1.060048 | 50311168 | 7.257335 | 43.038858 |

## First optimization pass

Partition the memory-mapped log at complete line boundaries. Each worker interns observed endpoints and accumulates exact latency samples locally. Merge the buckets by hour and endpoint, sort the combined latency samples, and emit independent hourly buffers in key order. The candidate matched the full public output hash and the official Python oracle on three additional tiny datasets (seeds 47, 103 and 327).

Run `run-057f7c74f4dc` on the organizer's full public input scored `6fd666fe2789` at **67.571535 practice points**. All gates passed. Wall time was 0.079430 s, peak memory 52,367,360 bytes, and the efficiency coefficient was 22.461084.

## Second optimization pass

The shard-local endpoint dictionary now uses open addressing with exact key comparisons. Small latency groups are sorted. Large groups are partitioned in ascending percentile order with `std::nth_element`. Two native paired full-data comparisons measured speedups of 1.36× and 1.54× over the preceding parallel version.
