# Protocol Specification

## 1. Log Ingestion
### 1.1 Transport
- TCP, stream-oriented.
- Server listens on configurable port (default 9999).【F:c/src/server.c†L1-L197】【F:cpp/src/LogServer.cpp†L1-L320】
- No TLS in MVP scope.

### 1.2 Message Format
- Client sends UTF-8 text lines terminated by `\n`.
- Server truncates payloads longer than 1024 bytes, appending `...` before storage.【F:c/src/server.c†L1-L120】
- No framing beyond newline; binary data is unsupported.

### 1.3 Delivery Semantics
- Logs are enqueued to the in-memory buffer immediately.
- When persistence is active, each accepted log is enqueued to the async writer queue before returning to idle.【F:c/src/server.c†L60-L120】【F:cpp/src/LogServer.cpp†L200-L320】
- Back-pressure occurs only when OS-level socket buffers fill; server does not send acknowledgements.

## 2. Query Interface
### 2.1 Transport & Lifecycle
- TCP listener on port 9998.
- Request/response per connection (no pipelining). Server closes socket after responding.【F:c/src/query_handler.c†L1-L120】

### 2.2 Commands
- `COUNT` – returns `COUNT: <n>`.
- `STATS` – returns summary `STATS: Total=<total>, Dropped=<dropped>, Current=<size>[, Clients=<count>]` (C++ includes client count via atomic state).【F:cpp/src/QueryHandler.cpp†L120-L200】
- `HELP` – prints multi-line usage instructions covering enhanced syntax.【F:c/src/query_handler.c†L60-L120】【F:cpp/src/QueryHandler.cpp†L160-L200】
- `QUERY` – accepts parameter tokens separated by spaces. Supported pairs:
  - `keyword=<text>` single substring.
  - `keywords=a,b,c` multiple substrings combined with `operator=AND|OR` (AND default).【F:c/src/query_parser.c†L40-L200】【F:cpp/src/QueryParser.cpp†L40-L200】
  - `regex=<pattern>` POSIX (C) or ECMAScript extended (C++).
  - `time_from=<unix>` / `time_to=<unix>` filtering by entry timestamp.

### 2.3 Error Responses
- Parser issues `ERROR: Invalid query syntax` or `ERROR: Search failed` when parsing or search fails.【F:c/src/query_handler.c†L60-L120】
- Unknown command returns `ERROR: Unknown command. Use HELP for usage.`

## 3. Persistence Protocol
### 3.1 File Layout
- Active file `current.log` plus rotated files named with timestamps (e.g., `YYYY-MM-DD-HHMMSS.log`).【F:c/src/persistence.c†L1-L200】【F:cpp/src/Persistence.cpp†L1-L200】
- Each line: `[YYYY-MM-DD HH:MM:SS] message`.

### 3.2 Rotation
- Triggered when file exceeds configured size (bytes). C++ implementation cleans old files per `max_files`; C version leaves TODO for cleanup.【F:c/src/persistence.c†L200-L360】【F:cpp/src/Persistence.cpp†L200-L320】

### 3.3 Startup Recovery
- Persistence manager may replay disk contents into memory using callback hooks (`persistence_load` in C, `PersistenceManager::load` template in C++).【F:c/src/persistence.c†L320-L400】【F:cpp/include/Persistence.h†L120-L190】

## 4. IRC Protocol (C++ MVP6)
### 4.1 Transport
- TCP on configurable port (default 6667).
- Standard IRC handshake: client must send `PASS` (optional), `NICK`, `USER` before receiving welcome.【F:cpp/include/IRCCommandHandler.h†L1-L120】

### 4.2 Command Coverage
- Core RFC commands: NICK, USER, JOIN, PART, PRIVMSG, NOTICE, QUIT, PING/PONG, TOPIC, NAMES, LIST, KICK, MODE, WHO, WHOIS.【F:cpp/include/IRCCommandHandler.h†L1-L120】
- Server-specific commands tunneled through PRIVMSG `!query`, `!logfilter`, `!logstream`, `!logstats` to interact with LogBuffer and QueryHandler.【F:cpp/include/IRCCommandHandler.h†L60-L120】【F:cpp/src/IRCChannelManager.cpp†L200-L320】

### 4.3 Channel Semantics
- Default log channels (#logs-all, #logs-error, #logs-warning, #logs-info, #logs-debug) stream entries whose `LogEntry.level` matches filter criteria.【F:cpp/versions/mvp5-irc/commit.md†L1-L120】【F:cpp/include/IRCChannel.h†L1-L120】
- Additional ad-hoc channels can be created by clients; log channels (#logs-*) are reserved and auto-managed.【F:cpp/src/IRCChannelManager.cpp†L1-L200】

### 4.4 Message Flow
See [docs/Diagrams/sequence-irc.txt](Diagrams/sequence-irc.txt) for the JOIN → log streaming sequence.

## 5. Internal Coordination
- `LogBuffer::addLogWithNotification` pushes events to registered callbacks (IRC channel manager).【F:cpp/include/LogBuffer.h†L80-L140】
- `IRCChannelManager::registerLogCallback` installs broadcaster; `distributeLogEntry` applies per-channel filters before broadcasting to subscribed clients.【F:cpp/src/IRCChannelManager.cpp†L200-L320】

