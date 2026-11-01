# Track C · Join

The program joins each record to its country and emits sorted per-country totals. Money stays in integer cents. Region IDs from the lookup table index an array. Unknown regions share the unmatched group.

Up to eight readers split the input at CRLF record boundaries. Track C section 2.2 guarantees that CR occurs only in a terminator, so an embedded LF in a quoted note cannot split a record. Each reader processes 256 KiB blocks and carries any partial record into the next block. Distinct names use the bytes between delimiters. An embedded quote has exactly one CSV encoding, a doubled quote, so comparing these views preserves exact name identity. Decoded byte lengths exclude the duplicate quote in each escape.

Workers accumulate numeric results locally. They share one exact-key name dictionary. Each name has a bit per country, and an atomic bit update counts each country/name pair once. Names and bit arrays are allocated in worker-owned arenas and remain valid until all readers finish.

Build and run on Linux with a C++17 compiler. The implementation uses the standard library and system threads.

```sh
g++ -std=c++17 -O3 -pthread -static-libstdc++ -static-libgcc track-c/join.cpp -o /tmp/join
/tmp/join /path/to/data /path/to/out
```

The Docker entry point reads `/data/records.csv` and `/data/regions.csv`, and writes `/out/aggregates.txt`. With another 24 hours, benchmark worker count and shared-dictionary cache misses on the judging CPU.
