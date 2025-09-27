# Sequence Map

Global SEQ identifiers will replace the legacy `[SEQUENCE: n]` markers. Each row maps the historical annotations to the future numbering plan per MVP. Exact SEQ values will be assigned during implementation but the ranges below reserve contiguous slots.

## C Track
| Legacy Stage | Legacy Range | Planned SEQ Range | Target MVP Deliverable |
|--------------|--------------|-------------------|------------------------|
| MVP1 – Basic TCP server | 1–32 | SEQ0000–SEQ0019 | Single-threaded select() accept loop, signal handling, CLI bootstrap.【F:c/versions/mvp1-basic-tcp-server/commit.md†L1-L200】 |
| MVP2 – Thread pool + buffer + query port | 61–148 | SEQ0020–SEQ0069 | Thread pool, circular buffer, query service, metrics.【F:c/versions/mvp2-c-thread-pool/commit.md†L1-L200】 |
| MVP3 – Enhanced query | 239–266 | SEQ0070–SEQ0099 | Query parser, regex/time filters, HELP docs.【F:c/versions/mvp3-enhanced-query/commit.md†L1-L200】 |
| MVP4 – Persistence | 302–347 | SEQ0100–SEQ0139 | Async persistence manager, CLI flags, rotation, load hooks.【F:c/versions/mvp4-persistence/commit.md†L1-L200】 |
| MVP5 – Security fixes | 348–413 | SEQ0140–SEQ0179 | Safe parsing, mutexed counters, truncation, buffer hardening.【F:c/versions/mvp5-security-fixes/commit.md†L1-L200】 |

## C++ Track
| Legacy Stage | Legacy Range | Planned SEQ Range | Target MVP Deliverable |
|--------------|--------------|-------------------|------------------------|
| MVP1 – OOP baseline | 33–60 | SEQ0180–SEQ0199 | RAII LogServer, Logger abstraction, CLI skeleton.【F:cpp/versions/mvp1-cpp-oop-design/commit.md†L1-L200】 |
| MVP2 – Modern threading & buffer | 149–238 | SEQ0200–SEQ0249 | ThreadPool with futures, LogBuffer (deque), QueryHandler.【F:cpp/versions/mvp2-cpp-modern-threading/commit.md†L1-L200】 |
| MVP3 – Enhanced query | 270–301 | SEQ0250–SEQ0289 | `QueryParser`, regex/time filters, HELP command.【F:cpp/versions/mvp3-enhanced-query/commit.md†L1-L200】 |
| MVP4 – Persistence | 354–403 | SEQ0290–SEQ0339 | `PersistenceManager`, CLI flags, cleanup routines.【F:cpp/versions/mvp4-persistence/commit.md†L1-L200】 |
| MVP5 – IRC foundation | 500–735 | SEQ0340–SEQ0419 | IRC server, channel/client managers, log streaming channels.【F:cpp/versions/mvp5-irc/commit.md†L1-L200】【F:cpp/include/IRCChannel.h†L1-L120】 |
| MVP6 – IRC integrations & polish | 736–879 | SEQ0420–SEQ0479 | Command handlers (`!query`, `!logstream`), channel stats, signal handling, CLI toggles.【F:cpp/include/IRCCommandHandler.h†L1-L120】【F:cpp/src/main.cpp†L1-L200】 |

## Notes
- Ranges leave slack for additional sequences (e.g., migrations, tests) without breaking ordering.
- Subsequent maintenance sequences (post-MVP) should continue incrementally beyond SEQ0479.
- Shared infrastructure (tests, scripts, CI) will consume SEQ identifiers after both tracks achieve MVP1 parity.

