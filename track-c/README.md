# Track C · Join

This program joins records to countries by region ID and writes one aggregate row for each country with at least one record. A dense table maps the bounded region IDs to country indices. Missing IDs map to the unmatched group. A single-pass CSV decoder consumes one record at a time, preserves commas, embedded newlines and escaped quotes in text, skips byte-order marks only at record boundaries, and sums money in integer cents.

A distinct name receives one integer ID across the file. Each country stores a bitset indexed by name ID, so a popular name costs one bit per country after its string has been interned. Notes are counted as decoded bytes without being stored. Countries are emitted in bytewise order.

No third-party dependencies are required. From the repository root, build and run with a C++17 compiler on Linux.

```sh
g++ -std=c++17 -O3 -static-libstdc++ -static-libgcc track-c/join.cpp -o /tmp/join
/tmp/join /path/to/data /path/to/out
```

The container entry point reads `/data/records.csv` and `/data/regions.csv`, and writes `/out/aggregates.txt`. With another 24 hours, measure name interning and CSV decoding separately on the scoring machine before changing either implementation.
