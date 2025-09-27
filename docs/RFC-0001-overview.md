# RFC-0001: LogCrafter Rebuild Overview

## 1. Purpose
This RFC captures the unified plan for recreating the LogCrafter training project after the legacy C and C++ tracks were archived. It consolidates requirements from the historical snapshots into a single specification that guides fresh implementations while preserving the MVP-based learning journey and the global SEQ sequence annotations.

## 2. Scope
- **Tracks**: C (5 MVP stages) and C++ (6 MVP stages with IRC extension).
- **Domains**: TCP log ingestion, query interface, optional persistence, structured logging, and real-time streaming over IRC.
- **Outputs**: Production-quality reference implementations, frozen snapshots per MVP, automated tests, and CI.

## 3. Legacy Recap
- The C track evolves from a select()-based server to a threaded reactor with persistence and security fixes.【F:c/src/server.c†L1-L197】【F:c/src/persistence.c†L1-L400】
- The C++ track mirrors functionality using RAII, modern STL, and adds an IRC subsystem in MVP6.【F:cpp/src/LogServer.cpp†L1-L200】【F:cpp/src/IRCChannelManager.cpp†L1-L200】
- Sequence annotations in both codebases document chronological learning milestones and must be preserved when re-implementing features.

## 4. System Architecture
The platform runs two sibling binaries (C and C++) with parallel capabilities. Each server exposes:

- **Log Port (default 9999)** – accepts newline-delimited log lines from TCP clients.
- **Query Port (default 9998)** – responds to COUNT/STATS/QUERY/HELP requests with textual payloads.
- **Optional Persistence** – flushes in-memory logs to disk with rotation.
- **IRC Service (C++ MVP6)** – publishes filtered log streams over RFC2812-compatible channels.

### 4.1 Component Structure (shared)
~~~text
+-------------------+      +---------------------+
|  TCP Log Clients  |      |   Query Clients     |
| (producers)       |      | (operators/BI)      |
+---------+---------+      +----------+----------+
          |                           |
          v                           v
    +-----------+               +-----------+
    | Accept    |               | Accept    |
    | Loop      |               | Loop      |
    +-----+-----+               +-----+-----+
          |                           |
          v                           v
    +-----------+               +-----------------+
    | Thread    |<--------------| Thread Pool     |
    | Pool      | Jobs          | (I/O workers)   |
    +-----+-----+               +--------+--------+
          |                               |
          v                               v
    +-----------+                 +--------------+
    | LogBuffer |<----------------| Query Engine |
    | (TS ring) |   search/load   | (parser + IO)|
    +-----+-----+                 +--------------+
          |
          v
    +-----------+
    | Persistence|
    | (optional) |
    +-----------+
~~~

### 4.2 C vs C++ Implementation Notes
- C uses pthreads, manual memory management, and POSIX file APIs.【F:c/include/thread_pool.h†L1-L66】【F:c/src/persistence.c†L1-L200】
- C++ wraps the same behavior in classes with RAII, std::thread, std::deque, std::regex, and std::filesystem.【F:cpp/include/ThreadPool.h†L1-L80】【F:cpp/include/Persistence.h†L1-L120】
- C++ MVP6 adds IRCServer composed of IRCChannel/Client managers and command handlers integrated with LogBuffer callbacks.【F:cpp/include/IRCServer.h†L1-L70】【F:cpp/src/IRCChannelManager.cpp†L1-L200】

## 5. Functional Overview by MVP
Refer to [docs/SequenceMap.md](SequenceMap.md) for a stage-by-stage crosswalk between legacy milestones, SEQ ranges, and the rebuild roadmap.

## 6. Operational Requirements
- Support 100+ concurrent log clients (C) and 200+ (C++).【F:c/README.md†L160-L190】【F:cpp/README.md†L1-L150】
- Preserve HELP/QUERY semantics, including regex and time-filter syntax.【F:c/src/query_handler.c†L1-L120】【F:cpp/src/QueryHandler.cpp†L1-L120】
- Ensure persistence is optional and asynchronous, with safe shutdown semantics.【F:c/src/main.c†L1-L120】【F:cpp/src/Persistence.cpp†L1-L200】
- For IRC (C++ MVP6) expose default log channels (#logs-all, #logs-error, …) and support command routing via IRCCommandHandler.【F:cpp/versions/mvp5-irc/commit.md†L1-L120】【F:cpp/include/IRCCommandHandler.h†L1-L120】

## 7. Non-Functional Goals
- Deterministic builds via CMake, with lint/test targets.
- Structured logging and metrics hooks to aid learners.
- Strict error handling and defensive parsing (safe strtol/regex cleanup).【F:c/src/main.c†L1-L120】【F:c/src/query_parser.c†L1-L200】

## 8. Deliverables Summary
- Rebuilt codebase under `work/` with synchronized snapshots per MVP.
- Documentation (this RFC, API, Protocol, Data Model, Performance, Migration, Assumptions, Diagrams, Sequence map).
- Automated tests aligned with smoke/spec/integration tiers.
- CI workflow executing build + ctest suites for both tracks.

## 9. Open Questions
See [docs/Assumptions.md](Assumptions.md) for gaps and the interim assumptions required to continue planning.

## 10. Step B Planning Snapshot

### 10.1 Planned Build Targets
- **Global Meta Targets**
  - `logcrafter_all` – orchestrates configuration for every active track target.
  - `logcrafter_tests` – placeholder aggregation for future ctest integration suites.
- **C Track**
  - `logcrafter_c_mvp0` (bootstrap echo server skeleton).
  - `logcrafter_c_mvp1` (baseline TCP ingest + HELP/COUNT commands).
  - `logcrafter_c_mvp2` (thread-pool reactor and concurrent query workers).
  - `logcrafter_c_mvp3` (enhanced query language and filtering).
  - `logcrafter_c_mvp4` (persistence pipeline and recovery).
  - `logcrafter_c_mvp5` (security/robustness hardening release).
- **C++ Track**
  - `logcrafter_cpp_mvp0` (bootstrap echo server skeleton).
  - `logcrafter_cpp_mvp1` (RAII TCP ingest baseline).
  - `logcrafter_cpp_mvp2` (modern threading + asynchronous jobs).
  - `logcrafter_cpp_mvp3` (query extensions and regex/time filters).
  - `logcrafter_cpp_mvp4` (persistence + structured logging integration).
  - `logcrafter_cpp_mvp5` (stability/security parity with C track).
  - `logcrafter_cpp_mvp6` (IRC real-time streaming subsystem).

### 10.2 Target Directory Layout (Planned)
```
./
├── CMakeLists.txt
├── docs/
├── work/
│   ├── c/
│   └── cpp/
├── snapshots/
│   ├── c/
│   └── cpp/
├── tests/
├── scripts/
└── ci/
```

The directories listed above remain empty until the corresponding build stages introduce real content (per the Step B staging instructions). Snapshots will be created only after MVP builds and tests pass.

