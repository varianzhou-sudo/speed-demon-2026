# Track A · Ingest

Partition the memory-mapped log at complete line boundaries. Each worker interns observed endpoints and accumulates exact latency samples locally. Merge the buckets by hour and endpoint, sort the combined latency samples, and emit independent hourly buffers in key order.

The implementation reads the log once, grouping requests by the first 13 timestamp bytes (the UTC hour) and the exact endpoint. It keeps each group's latency values and its count of HTTP 5xx responses. Ordered maps put the groups in output order. Sorting the latencies within a group gives exact nearest-rank percentiles, including when a latency or an entire input row is duplicated. Error rates are computed as integer parts per million.

Build locally with a C++17 compiler and its standard library. There are no third-party runtime dependencies.

```sh
g++ -pthread -std=c++17 -O2 track-a/ingest.cpp -o /tmp/ingest
/tmp/ingest /path/to/data/requests.log /tmp/summary.txt
```

The Docker entry point reads `/data/requests.log` and writes `/out/summary.txt`. With another 24 hours, profile shard-local bucket allocation and the final merges on the scoring machine.
