# Change Log

## SEQ0000 – C Track MVP0 bootstrap
- Introduced the MVP0 LogCrafter C server skeleton with log and query listener scaffolding.
- Added snapshot tooling to freeze MVP progress for future stages.

## SEQ0001–SEQ0003 – C Track MVP1 baseline server
- Added in-memory log storage with COUNT/STATS/QUERY keyword handling on the query port.
- Updated the single-threaded loop to persist logs for later queries and emit user-facing help text.
- Switched to short-form CLI parsing (`-p`, `-h`) and maintained graceful signal-driven shutdown semantics.

## SEQ0004–SEQ0010 – C Track MVP2 thread-pooled server
- Introduced a pthread-based worker pool and mutex-guarded circular buffer for concurrent log ingestion.
- Updated the query pipeline to stream keyword matches using shared buffer snapshots and to report active client metrics.
- Refreshed the CLI entrypoint and build target to expose the MVP2 binary while keeping the MVP1 alias for snapshots.

## SEQ0011–SEQ0016 – C Track MVP3 enhanced query
- Added a dedicated query parser supporting keyword lists, AND/OR operators, regex filters, and time ranges.
- Extended the log buffer to evaluate advanced filters under mutex protection and emit timestamped results for clients.
- Upgraded the thread-pooled server and build target to surface MVP3 banners, HELP guidance, and formatted query responses.

## SEQ0017–SEQ0023 – C Track MVP4 persistence
- Connected the C server runtime to an asynchronous persistence manager with replay hooks and retention policy controls.
- Added configurable CLI toggles for enabling persistence, selecting directories, and defining rotation thresholds.
- Captured persistence statistics inside the STATS command and promoted the active build to the MVP4 target.

## SEQ0024–SEQ0029 – C Track MVP5 security hardening
- Added configurable client capacity limits, mutex-guarded reservation counters, and scrubbed log ingestion to prevent overflow.
- Hardened the circular buffer and query parser with bounded copies, keyword caps, and regex size limits to defend against abuse.
- Promoted the build output to `logcrafter_c_mvp5`, expanded STATS reporting with capacity metrics, and exposed a CLI flag for overrides.

## SEQ0030 – C++ Track MVP0 bootstrap
- Introduced the baseline C++ server skeleton with mirrored log/query listener configuration defaults.
- Implemented the select-driven event loop plus simple log ingestion and HELP responses for MVP0 clients.
- Wired the `logcrafter_cpp_mvp0` build target into the root project to enable future snapshots.

## SEQ0031–SEQ0033 – C++ Track MVP1 keyword query server
- Added fixed-capacity in-memory storage with rolling counters so the C++ server can retain logs for COUNT/STATS reporting.
- Streamed keyword-based query results with helpful HELP/ERROR responses and preserved MVP0 logging semantics.
- Promoted the active binary to `logcrafter_cpp_mvp1` while keeping the MVP0 build alias for snapshot compatibility.

## SEQ0034–SEQ0040 – C++ Track MVP2 threaded server
- Introduced a mutex-guarded circular log buffer module and std::thread worker pool for concurrent client handling.
- Updated the MVP2 server to dispatch log/query sessions through the pool with active client metrics and configurable capacity.
- Extended the CLI/build wiring to surface the `logcrafter_cpp_mvp2` binary with worker and buffer tuning flags.

## SEQ0041–SEQ0046 – C++ Track MVP3 advanced query
- Added a reusable query parser that accepts keywords, AND/OR operators, regex filters, and optional time ranges for C++ clients.
- Extended the log buffer to capture timestamps and execute compound filters while emitting formatted query results.
- Promoted the active binary to `logcrafter_cpp_mvp3`, refreshed CLI messaging, and routed query handling through the new parser.

## SEQ0047–SEQ0053 – C++ Track MVP4 persistence
- Introduced an asynchronous persistence manager with rotation, pruning, and replay callbacks plus CLI controls for directory and limits.
- Taught the log buffer and server runtime to ingest timestamped replays, expose persistence metrics, and stream MVP4 banners.
- Promoted the build output to `logcrafter_cpp_mvp4` and refreshed the CMake wiring to include the new persistence component.

## SEQ0054–SEQ0064 – C++ Track MVP5 IRC foundation
- Added an embedded IRC server with channel, client, and command management to stream logs into default `#logs-*` channels.
- Linked log ingestion and persistence replay to broadcast through IRC while surfacing active IRC counts in STATS responses.
- Promoted the runnable target to `logcrafter_cpp_mvp5` with CLI toggles for enabling the IRC listener and custom port selection.

## SEQ0065–SEQ0075 – C++ Track MVP6 IRC polish
- Introduced an IRC command handler for `!query`, `!logstream`, `!logfilter`, and `!logstats`, plus CLI controls for server name and auto-join channels.
- Enhanced the IRC server with LIST/NAMES/TOPIC responses, channel statistics, custom filter channels, and richer runtime metrics.
- Promoted the build to `logcrafter_cpp_mvp6`, refreshed welcome banners, and hardened signal handling for production-style runs.

## SEQ0076 – Step C test and CI plan
- Documented the Step C strategy covering smoke, spec, and integration suites with SEQ mappings across both tracks.
- Established sequencing for upcoming automation work and identified scripts/fixtures required for CI integration.

## SEQ0079–SEQ0083 – Step C smoke automation
- Added the shared Python harness and CMake wiring to register Step C smoke scenarios with `ctest`.
- Implemented bind/conflict, dual-listener, shutdown, capacity, IRC boot, and persistence toggle smoke tests for C and C++.
- Delivered a reusable `run_smoke.sh` helper and refreshed testing docs to guide developers through executing the suite.

## SEQ0084–SEQ0092 – Step C spec protocol suite
- Declared the spec test package and harness implementing protocol happy-path, invalid input, partial I/O, timeout, and SIGINT shutdown cases across both tracks.
- Registered the spec runner with CTest under the `spec` label and documented execution guidance in the testing README.
- Ensured each spec scenario references the historical SEQ ranges it verifies, preserving traceability to the MVP implementations.

## SEQ0093–SEQ0101 – Step C integration automation
- Introduced the integration test package with broadcast, IRC, and lifecycle scenarios validating concurrent workflows across the C and C++ servers.
- Wired the integration harness into CTest, added a dedicated runner script, and documented execution guidance alongside the smoke/spec suites.
- Supplied a lightweight load generator under `tools/` to replay log streams during integration and lifecycle validation.

## SEQ0102–SEQ0104 – Step C CI workflow
- Added the GitHub Actions pipeline that checks out the repo, configures CMake, builds all targets, and runs the entire `ctest` suite.
- Documented the job sequence in `ci/README.md` so contributors understand the automated validation stages.
