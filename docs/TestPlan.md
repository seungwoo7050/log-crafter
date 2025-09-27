# Step C Test and CI Plan

## Objectives
- Expand automated validation beyond Step B smoke checks to cover reliability, persistence, security, and IRC behaviour for both C and C++ tracks.
- Align every planned test with the feature sequences (SEQ ids) that introduced the behaviour under test.
- Provide runnable suites (`ctest`) and helper scripts (`scripts/run_smoke.sh`, `scripts/run_integration.sh`) that developers and CI can share.

## Test Environment
- Build system: CMake (existing root project).
- Runners: GitHub Actions Ubuntu LTS and local Linux development hosts.
- Common tooling: `ctest` orchestration, Python 3.10+ harness for network integration, bash helper scripts for orchestration.
- Servers under test: `logcrafter_c_mvp5` (latest C track) and `logcrafter_cpp_mvp6` (latest C++ track).

## Suite Overview
| Suite        | Purpose                                                                 | Primary Artifacts |
|--------------|--------------------------------------------------------------------------|-------------------|
| Smoke        | Fast validation of server bootstrap, binding, shutdown, and CLI guardrails. | `tests/smoke/`, `scripts/run_smoke.sh` |
| Spec         | Deterministic verification of modules (buffer, parser, persistence) via focused unit-like tests. | `tests/spec/` |
| Integration  | End-to-end coverage of log ingestion, replay, IRC bridging, and concurrent workflows. | `tests/integration/`, `scripts/run_integration.sh` |

All suites will register with `ctest` under the labels `smoke`, `spec`, and `integration` and execute within the shared `build` tree.

## Smoke Suite (Extended)
| Test ID | Description | Tracks | Related SEQ ids |
|---------|-------------|--------|-----------------|
| smoke_bind_conflict | Attempt sequential startups on occupied ports to confirm graceful refusal and retry guidance. | C, C++ | SEQ0000, SEQ0030 |
| smoke_dual_listener | Verify log and query listeners bind simultaneously and reject mismatched CLI pairs. | C, C++ | SEQ0000–SEQ0003, SEQ0030–SEQ0033 |
| smoke_shutdown_signal | Send SIGINT/SIGTERM to ensure orderly worker teardown and snapshot-friendly exit codes. | C, C++ | SEQ0000–SEQ0023, SEQ0030–SEQ0053 |
| smoke_max_clients | Stress login limit using short-lived sockets to confirm reservation counters and rejection banners. | C | SEQ0024–SEQ0029 |
| smoke_irc_boot | Launch IRC listener, await MOTD, join default channels, and ensure `!help` banner appears. | C++ | SEQ0054–SEQ0075 |
| smoke_persistence_toggle | Toggle persistence on/off via CLI, ensuring directories are created or skipped correctly. | C, C++ | SEQ0017–SEQ0023, SEQ0047–SEQ0053 |

## Spec Suite
Focused tests with deterministic inputs, often using harness binaries or direct library calls.

| Test ID | Description | Tracks | Related SEQ ids |
|---------|-------------|--------|-----------------|
| spec_log_buffer_bounds | Validate max message length, FIFO retention, and statistic counters on the in-memory buffer. | C, C++ | SEQ0001–SEQ0003, SEQ0031–SEQ0033 |
| spec_thread_pool_sizing | Simulate high/low worker counts to ensure queues drain without starvation and respect capacity limits. | C | SEQ0004–SEQ0010 |
| spec_thread_pool_cpp | Mirror of above using std::thread implementation with timing assertions. | C++ | SEQ0034–SEQ0040 |
| spec_query_parser_matrix | Feed combined keyword/regex/time window inputs to guarantee safe parsing and bounded errors. | C, C++ | SEQ0011–SEQ0016, SEQ0041–SEQ0046 |
| spec_persistence_rotation | Inject synthetic timestamps to ensure rotation thresholds prune and replay ordering is stable. | C, C++ | SEQ0017–SEQ0023, SEQ0047–SEQ0053 |
| spec_security_scrub | Confirm sanitisation removes unsafe characters and enforces regex/keyword caps. | C | SEQ0024–SEQ0029 |
| spec_irc_command_parser | Validate parsing of `!query`, `!logstream`, `!logfilter`, and `!logstats` payloads. | C++ | SEQ0054–SEQ0075 |

## Integration Suite
End-to-end flows using real binaries and network sockets.

| Test ID | Description | Tracks | Related SEQ ids |
|---------|-------------|--------|-----------------|
| integ_log_roundtrip | Send logs over TCP, execute keyword queries, and confirm STATS counts with persistence disabled. | C, C++ | SEQ0000–SEQ0046 |
| integ_persistence_replay | Write logs with persistence enabled, restart server, and verify replayed entries service queries. | C, C++ | SEQ0017–SEQ0023, SEQ0047–SEQ0053 |
| integ_security_load | Launch concurrent clients near max capacity performing mixed queries to ensure graceful rejection and queue metrics. | C | SEQ0004–SEQ0029 |
| integ_regex_time_filter | Submit logs with timestamps, query with regex + time ranges, and ensure consistent ordering across tracks. | C, C++ | SEQ0011–SEQ0046 |
| integ_irc_stream_bridge | Feed log stream and verify IRC channel broadcasts and `!logstream`/`!logfilter` behaviour for multiple clients. | C++ | SEQ0054–SEQ0075 |
| integ_ci_matrix | Combined scenario executing smoke + spec subsets sequentially to ensure GitHub Actions runtime viability. | C, C++ | All prior SEQ ids |

## Test Data and Utilities
- Temporary directories under `${CTEST_BINARY_DIR}/tmp` for persistence and IRC logs, cleaned after runs.
- Python harnesses will reuse utility modules for socket operations, CLI launching, and IRC message parsing.
- Shared fixtures in `tests/common/` (to be created) for configuration defaults and assertions.

## CI Integration
- Add `ctest` labels to orchestrate GitHub Actions jobs: `smoke` (fast), `spec` (medium), `integration` (full, nightly gate).
- Use matrix job to exercise both tracks with and without persistence enabled.
- Publish key artifacts (server logs, IRC transcripts) on failure for debugging.

## Next Steps
1. Seek approval for this plan.
2. Implement shared test utilities and `ctest` registration (SEQ0076–SEQ0078).
3. Build smoke suite and helper script updates (SEQ0079–SEQ0083).
4. Add spec suite coverage (SEQ0084–SEQ0090).
5. Implement integration suite and CI workflow (SEQ0091–SEQ0098).
6. Update `CHANGES.md` and documentation alongside each sequence block.
