# Track A performance log

This local experiment replaces Python timestamp parsing with a fixed-width UTC-hour key. The C++17 implementation streams the input, stores unsigned 32-bit latencies, and sorts the values in each hour/endpoint group. Both versions write and close their output before the timer stops.

| Local workload | Official Python reference | C++17 candidate | Comparison |
|---|---:|---:|---|
| 310,000 rows, sample seed 1 | 5.821 s | 0.236 s | Median of three process wall times per implementation, run order alternated, warm filesystem cache |

Individual runs in seconds were 5.821, 3.507 and 8.543 for Python, and 0.181, 0.236 and 0.361 for C++. These times came from a shared Linux host. Both programs wrote 66,153 identical groups with SHA-256 `ca6311bbeb536fa55d85265bf6fd3d24cc9586f13de1ac73bd2a23ac3893aac3`, matching the published sample checksum.

No public board run has been submitted. Record official run IDs and the measured effects here after authorized submission. No official speed, memory, or score is available.

On the full public seed 1, the implementation processed 3,100,000 input records and wrote 126,908 groups. The output matched the organizers' full SHA-256, `495ca09682b9673ef836a506496177680bb4a3af54255174befddb1f83481cd3`. An independent 1,000-row tiny dataset at seed 19 also matched the official Python reference byte for byte. Empty input produced the required empty output. The official Dockerfile policy check passed. A local static build used GCC 13.3.0; the pinned GCC 15 Docker build and official Docker timing remain unverified.
