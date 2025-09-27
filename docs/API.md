# API Specification

## 1. Command-Line Interfaces
### 1.1 C Track Binary (`logcrafter-c`)
Options mirror the legacy behavior.【F:c/src/main.c†L1-L120】

| Option | Description | Default |
|--------|-------------|---------|
| `-p PORT` | Set log listener TCP port (also shifts query port to `PORT-1` when we modernize). Validate with `strtol`, 1–65535. | 9999 |
| `-P` | Enable persistence layer. | Disabled |
| `-d DIR` | Persistence directory path. | `./logs` |
| `-s SIZE_MB` | Max file size before rotation (MB). Checked for overflow. | 10 |
| `-h` | Print usage and exit. | — |

### 1.2 C++ Track Binary (`logcrafter-cpp`)
Adds IRC controls while keeping persistence flags identical.【F:cpp/src/main.cpp†L1-L200】

| Option | Description | Default |
|--------|-------------|---------|
| `-p PORT` | Log listener port. Query port remains 9998 until configurable. | 9999 |
| `-P` | Enable persistence. | Disabled |
| `-d DIR` | Persistence directory path. | `./logs` |
| `-s SIZE_MB` | Max rotated file size (MB). | 10 |
| `-i` | Enable IRC server on default port. | Off |
| `-I PORT` | Enable IRC server on custom port. | 6667 |
| `-h` | Print help and exit. | — |

## 2. Network APIs
### 2.1 Log Ingestion Port (TCP)
- Accepts plain-text lines terminated by `\n`.
- Logs exceeding the safe length (1,024 bytes) are truncated with `...` to protect memory.【F:c/src/server.c†L1-L120】
- Connections are handled by thread pools in both tracks.【F:c/src/server.c†L80-L197】【F:cpp/src/LogServer.cpp†L200-L320】

#### Behaviors
- Server closes connection on EOF or error.
- For C++ track with IRC enabled, ingested logs trigger channel distribution callbacks after buffering.【F:cpp/src/LogBuffer.cpp†L1-L200】【F:cpp/src/IRCChannelManager.cpp†L200-L320】

### 2.2 Query Port (TCP)
Simple request/response protocol using ASCII commands.【F:c/src/query_handler.c†L1-L200】【F:cpp/src/QueryHandler.cpp†L1-L200】

| Command | Response |
|---------|----------|
| `COUNT` | `COUNT: <n>` current logs buffered. |
| `STATS` | `STATS: Total=<total>, Dropped=<dropped>, Current=<size>[, Clients=<count>]`. |
| `HELP` | Multi-line usage summary including enhanced query syntax. |
| `QUERY keyword=foo ...` | `FOUND: <n>` followed by matching lines with timestamps. Accepts parameters described in [docs/Protocol.md](Protocol.md). |

### 2.3 IRC Service (C++ MVP6)
- RFC 2812 style handshake (NICK, USER) followed by JOIN/PART/etc.【F:cpp/include/IRCCommandHandler.h†L1-L120】
- LogCrafter-specific commands use `PRIVMSG` with `!` prefixes (e.g., `!query`, `!logstream`). Implementation wires into `IRCCommandHandler` helpers.【F:cpp/versions/mvp5-irc/commit.md†L1-L120】
- Channel catalog managed by `IRCChannelManager`; default channels stream filtered logs automatically.【F:cpp/src/IRCChannelManager.cpp†L1-L320】

## 3. Internal APIs
### 3.1 Thread Pool
- **C**: `thread_pool_add_job(pool, fn, arg)` enqueues tasks executed by worker threads; `thread_pool_wait` blocks for shutdown.【F:c/include/thread_pool.h†L1-L66】【F:c/src/thread_pool.c†L1-L200】
- **C++**: `ThreadPool::enqueue` returns `std::future` for async results; RAII handles worker lifetime.【F:cpp/include/ThreadPool.h†L1-L120】【F:cpp/src/ThreadPool.cpp†L1-L120】

### 3.2 Log Buffer
- **C**: `log_buffer_push`, `log_buffer_search_enhanced` operate on a mutex-protected circular buffer with timestamped entries.【F:c/include/log_buffer.h†L1-L80】【F:c/src/log_buffer.c†L1-L260】
- **C++**: `LogBuffer::push`, `searchEnhanced`, plus IRC callbacks via `addLogWithNotification`. Uses `std::deque` and `std::chrono`.【F:cpp/include/LogBuffer.h†L1-L120】【F:cpp/src/LogBuffer.cpp†L1-L200】

### 3.3 Persistence Layer
- **C**: `persistence_write`, `persistence_flush`, `persistence_rotate`, `persistence_load`. Async queue processed by writer thread.【F:c/include/persistence.h†L1-L80】【F:c/src/persistence.c†L1-L360】
- **C++**: `PersistenceManager::write`, `flush`, `load`, `getStats`. Uses `std::filesystem` and RAII-managed threads.【F:cpp/include/Persistence.h†L1-L160】【F:cpp/src/Persistence.cpp†L1-L200】

### 3.4 Query Parsing
- **C**: `query_parser_parse` populates `parsed_query_t` with keywords, regex, and time filters; integrates with log buffer search.【F:c/include/query_parser.h†L1-L80】【F:c/src/query_parser.c†L1-L200】
- **C++**: `QueryParser::parse` returns `ParsedQuery` containing `std::optional` filters and compiled `std::regex`.【F:cpp/include/QueryParser.h†L1-L120】【F:cpp/src/QueryParser.cpp†L1-L200】

### 3.5 IRC Integration Hooks (C++)
- `IRCServer::setLogBuffer` wires real-time events; `IRCChannel::setLogFilter` applies per-channel filtering logic.【F:cpp/include/IRCServer.h†L1-L70】【F:cpp/include/IRCChannel.h†L1-L120】
- `IRCChannelManager::registerLogCallback` allows central distribution of `LogEntry` objects to interested channels or bots.【F:cpp/src/IRCChannelManager.cpp†L200-L320】

## 4. Snapshot & Sequence APIs
- Each MVP snapshot is a full copy of `work/<track>`; no in-place edits inside `snapshots/` once created.
- Source files carry header comments enumerating the SEQ id, track, MVP stage, change summary, and related tests (per instruction in AGENTS.md).

