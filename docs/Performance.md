# Performance Plan

## 1. Targets
| Track | Throughput | Latency | Concurrency | Source |
|-------|------------|---------|-------------|--------|
| C | ~50k logs/sec (memory only) | <1 ms average | 100+ clients | Legacy README.【F:c/README.md†L160-L200】 |
| C++ | ~60k logs/sec (memory only) | <0.5 ms average | 200+ clients | Legacy README.【F:cpp/README.md†L1-L150】 |

## 2. Performance-Critical Paths
1. **Socket Accept Loop** – select() + thread pool dispatch.【F:c/src/server.c†L1-L200】【F:cpp/src/LogServer.cpp†L1-L200】
2. **Log Buffer Operations** – push/search operations must remain O(1) amortized for enqueue and O(n) for scans with minimal copying.【F:c/src/log_buffer.c†L1-L200】【F:cpp/src/LogBuffer.cpp†L1-L200】
3. **Persistence Writer** – asynchronous queue ensures disk I/O is decoupled from hot path.【F:c/src/persistence.c†L1-L200】【F:cpp/src/Persistence.cpp†L1-L200】
4. **IRC Broadcast** – log distribution runs under shared locks; avoid per-client blocking operations.【F:cpp/src/IRCChannelManager.cpp†L200-L320】

## 3. Optimization Strategies
- Prefer fixed-size thread pools with minimal contention (single mutex/condvar) and avoid busy-wait loops.【F:c/src/thread_pool.c†L1-L200】【F:cpp/src/ThreadPool.cpp†L1-L120】
- Use move semantics in C++ (`LogBuffer::push(std::string&&)`) to reduce allocations.【F:cpp/src/LogBuffer.cpp†L1-L200】
- Batch disk writes and flush every interval rather than per message. Both persistence managers already accumulate queue entries before flush.【F:c/src/persistence.c†L1-L200】【F:cpp/src/Persistence.cpp†L1-L200】
- Keep regex compilation single-pass per query; reused by search loops.【F:c/src/query_parser.c†L1-L200】【F:cpp/src/QueryParser.cpp†L1-L200】

## 4. Benchmarking Plan
- **Smoke**: netcat-based scripts pushing ~1k logs to validate functionality.
- **Spec**: Multi-client Python scripts replicating `tests/test_concurrent.py` and query/persistence coverage.
- **Integration**: Combined log + query + IRC streaming scenario verifying latency under 200ms for query responses and sub-second propagation to IRC channels.

## 5. Resource Footprint
- Memory: 10,000-entry buffer uses ~100 MB (C) / ~80 MB (C++).【F:c/README.md†L160-L200】【F:cpp/README.md†L1-L150】
- Disk: Rotated logs sized by `max_file_size` (default 10 MB) with up to 10 retained files per config.
- CPU: Expect <50% usage on 4-core machine under nominal load thanks to asynchronous design.

## 6. Monitoring Hooks
- Add counters for total logs received, dropped, persisted, and IRC broadcasts (already tracked as stats counters in log buffer/persistence).【F:c/src/log_buffer.c†L1-L260】【F:cpp/include/Persistence.h†L1-L160】
- Expose CLI/IRC commands returning metrics (`STATS`, `!logstats`). Preserve textual output for backward compatibility while planning structured logging for Step B.

