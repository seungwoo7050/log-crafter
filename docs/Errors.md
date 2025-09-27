# Error Handling & Fault Model

## 1. Input Validation
- **Ports & Sizes**: Both tracks validate numeric CLI options using safe parsing (`strtol` in C, `std::stoi/stoul` in C++). Invalid values abort start-up with error messages.【F:c/src/main.c†L1-L120】【F:cpp/src/main.cpp†L1-L120】
- **Query Parameters**: Unknown parameter types are ignored gracefully; malformed values yield `ERROR: Invalid query syntax`. Regex compilation failures are reported and cleaned up to avoid leaks.【F:c/src/query_parser.c†L1-L200】【F:cpp/src/QueryParser.cpp†L1-L200】

## 2. Runtime Protections
- **Client Count**: C server guards `client_count` with a mutex after MVP5 to prevent overflow and rejects connections beyond `MAX_CLIENTS`.【F:c/src/server.c†L200-L320】
- **Message Size**: Log payloads are truncated to `SAFE_LOG_LENGTH` (1024 bytes) before pushing to the buffer.【F:c/include/log_buffer.h†L1-L60】【F:c/src/server.c†L1-L120】
- **Thread Pool Shutdown**: Workers honor shutdown flags and avoid dereferencing freed jobs by gating on queue state.【F:c/src/thread_pool.c†L1-L200】【F:cpp/src/ThreadPool.cpp†L1-L120】

## 3. Persistence Failure Modes
- Write failures increment error counters; manager continues processing subsequent entries.【F:c/src/persistence.c†L1-L200】【F:cpp/src/Persistence.cpp†L1-L200】
- Directory creation or file rotation exceptions bubble up during initialization (C++), preventing server start to avoid silent data loss.【F:cpp/src/Persistence.cpp†L1-L200】
- Flush/rotate operations guard against NULL file handles and log rename failures; rotation gracefully resets counters or retries where possible.【F:c/src/persistence.c†L200-L360】【F:cpp/src/Persistence.cpp†L200-L320】

## 4. IRC-Specific Errors (C++ MVP6)
- Invalid nick/user combinations trigger standard IRC numeric replies before disconnecting (to be preserved in rebuild).
- Attempts to join reserved log channels without authentication fail silently, preserving read-only semantics.【F:cpp/src/IRCChannelManager.cpp†L1-L200】
- Command parser enforces token limits; unknown commands default to generic error responses handled in `IRCCommandHandler`.

## 5. Logging & Observability
- Console logger emits timestamped INFO/ERROR messages for each connection lifecycle event.【F:cpp/src/Logger.cpp†L1-L80】
- C implementation uses `printf`/`perror` for the same milestones; rebuild must replace ad-hoc logging with structured macros to achieve parity.【F:c/src/server.c†L1-L200】

## 6. Recovery Strategy
- Persistence replay hooks (`persistence_load`, `PersistenceManager::load`) provide a mechanism to repopulate buffers after crash recovery; rebuild must surface CLI toggles to enable this behavior by MVP4.【F:c/src/persistence.c†L320-L400】【F:cpp/include/Persistence.h†L120-L190】
- IRC channel manager automatically reinitializes default log streams on startup, ensuring streaming resumes without manual intervention.【F:cpp/src/IRCChannelManager.cpp†L200-L320】

