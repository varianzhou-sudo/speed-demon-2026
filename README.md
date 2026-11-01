# Speed Demon 2026

Team `Super-Tangram!!` is preparing entries for three tracks of the [Speed Demon 2026](https://speeddemon.dev/) performance competition.

| Track | Task | Implementation |
| --- | --- | --- |
| A | Group HTTP requests by hour and endpoint and compute exact latency percentiles | [Ingest](track-a/) |
| B | Answer Boolean and phrase queries on a document collection | [Index](track-b/) |
| C | Stream CSV transactions, join region metadata and compute country aggregates | [Join](track-c/) |

Each track contains the source, a Dockerfile, and local benchmark notes in `PERF.md`. All three native programs matched the organizers' published full seed-1 output hashes. All three entries have also passed the organizer’s public practice machine. The track performance logs record the run IDs. Hidden-data results are pending.

## Reproduce

Download the relevant full dataset from the [event board](https://speeddemon.dev/board/). With Docker and the required base images available, run the organizer's checker from the repository root. For example, to verify Track A with data in `data/a`, use:

```sh
python3 check.py a --data data/a --expected 495ca09682b9673ef836a506496177680bb4a3af54255174befddb1f83481cd3
```

Development used AI coding assistance. See the track READMEs for algorithms and the [MIT license](LICENSE) for reuse.
