# Data Model

## 1. Core Entities
### 1.1 Log Entry
| Field | C Track | C++ Track | Notes |
|-------|---------|-----------|-------|
| Message | `char*` stored via `strdup` | `std::string` | Limited to 1,024 bytes before storage.【F:c/src/server.c†L1-L120】【F:cpp/src/LogBuffer.cpp†L1-L200】 |
| Timestamp | `time_t` | `std::chrono::system_clock::time_point` | Captured at enqueue time. |
| Metadata | None | `level`, `source`, `category`, `metadata` map for IRC filtering | Added in C++ MVP6 for channel routing.【F:cpp/include/LogBuffer.h†L1-L120】 |

### 1.2 Log Buffer
- **C**: circular array `log_entry_t**` with head/tail indices and stats counters.【F:c/include/log_buffer.h†L1-L80】【F:c/src/log_buffer.c†L1-L200】
- **C++**: `std::deque<LogEntry>` protected by mutexes and condition variables. Tracks totals via atomics.【F:cpp/include/LogBuffer.h†L1-L120】【F:cpp/src/LogBuffer.cpp†L1-L200】

Statistics tracked:
- `totalLogs`, `droppedLogs`, `current size`.
- Client count (C: tracked on server; C++: atomic in `LogServer`).【F:c/src/server.c†L200-L320】【F:cpp/src/LogServer.cpp†L1-L200】

### 1.3 Persistence Queue Entry
| Field | C | C++ | Notes |
|-------|---|-----|-------|
| Message | `char*` (duplicated) | `std::string` | Owned until write completes.【F:c/src/persistence.c†L1-L200】【F:cpp/src/Persistence.cpp†L1-L120】 |
| Timestamp | `time_t` | `std::chrono::system_clock::time_point` | Formatted on write. |
| Next Pointer | Singly linked list | `std::queue` | Implementation detail of async queue. |

### 1.4 IRC Domain (C++ MVP6)
| Entity | Description |
|--------|-------------|
| `IRCServer` | Owns accept loop, thread pool, managers, parser/handler objects.【F:cpp/include/IRCServer.h†L1-L70】 |
| `IRCClient` | Tracks connection state, nickname, channels, authentication (details derived from legacy source). |
| `IRCChannel` | Stores topic, modes, operator list, log filters, membership.【F:cpp/include/IRCChannel.h†L1-L120】 |
| `IRCChannelManager` | Map of channels, default log channel configs, log distribution callbacks.【F:cpp/src/IRCChannelManager.cpp†L1-L320】 |
| `IRCCommandParser` | Tokenizes IRC lines into `IRCCommand` structures (verb + params). |
| `IRCCommandHandler` | Dispatch table mapping commands to handlers; handles LogCrafter-specific commands.【F:cpp/include/IRCCommandHandler.h†L1-L120】 |

## 2. Sequence Numbering
- Global namespace `SEQ0000..` shared across both tracks.
- Legacy markers `[SEQUENCE: n]` align with future SEQ IDs via mapping in [SequenceMap](SequenceMap.md).
- Each header block in rebuilt source will follow the format mandated in AGENTS.md (Sequence, Track, MVP, Change, Tests).

## 3. Configuration
| Setting | Storage | Default | Notes |
|---------|---------|---------|-------|
| Thread count | Build-time default (4 in C, hardware concurrency in C++) | `DEFAULT_THREAD_COUNT` / `std::thread::hardware_concurrency()` | Expose CLI override later if needed.【F:c/include/thread_pool.h†L1-L66】【F:cpp/src/ThreadPool.cpp†L1-L80】 |
| Buffer capacity | `DEFAULT_BUFFER_SIZE` / `LogBuffer::DEFAULT_CAPACITY` | 10,000 entries | Should become configurable MVP3+. |
| Persistence directory | CLI | `./logs` | Created if missing; ensure permissions. |
| Persistence max files | Config struct | 10 | C implementation notes TODO for cleanup; C++ already cleans oldest logs.【F:c/src/persistence.c†L200-L360】【F:cpp/src/Persistence.cpp†L200-L320】 |
| IRC defaults | Hard-coded arrays | `#logs-*` | Derived from `IRCChannelManager::defaultLogChannels_`.【F:cpp/src/IRCChannelManager.cpp†L1-L120】 |

## 4. Data Flows
1. **Ingestion**: Socket → worker → buffer → (optional) persistence queue → disk.
2. **Query**: Socket → parser → buffer search → response.
3. **IRC**: LogBuffer hook → channel manager filter → channel broadcast.

## 5. Future Enhancements
- Persisted metadata for IRC (level/source) currently only stored in memory; consider serializing to disk by MVP6.
- Snapshot metadata should capture SEQ ranges and configuration at freeze time.

