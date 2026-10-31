# Index

The program answers left-associative Boolean queries and exact contiguous phrase queries. It reads the complete query file before scanning the corpus so that only terms appearing in a query get posting lists. Each document is tokenized once using byte-wise ASCII rules. Tokens absent from the query vocabulary get a shared sentinel in the per-document stream, preserving phrase boundaries.

Boolean evaluation merges sorted document ID lists. For phrases, the shortest posting list selects candidate documents. Each candidate's token stream is checked for the complete ordered phrase. Output is in ascending ID order; sums use 64-bit integers.

The offline container entrypoint reads `/data/docs.txt` and `/data/queries.txt` and writes `/out/results.txt`. To run natively with C++20 and the standard library:

```sh
g++ -O3 -std=c++20 main.cpp -o solve
./solve /path/to/inputs /path/to/outputs
```
