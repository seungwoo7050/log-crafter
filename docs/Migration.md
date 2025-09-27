# Migration & Divergence Notes

## 1. Legacy to Rebuild Strategy
- **Clean Slate**: Legacy code will be removed after documentation (per instructions). Rebuild will start inside `work/c` and `work/cpp` directories with modern project scaffolding.
- **Snapshots**: Each MVP stage will be captured by cloning `work/<track>` into `snapshots/<track>/mvpX` after tests pass, mirroring the previous version history.

## 2. Feature Parity Checklist
| Feature | C Track MVP | C++ Track MVP | Notes |
|---------|-------------|---------------|-------|
| Basic TCP server | MVP1 | MVP1 | select() vs RAII-managed server.【F:c/versions/mvp1-basic-tcp-server/commit.md†L1-L200】【F:cpp/versions/mvp1-cpp-oop-design/commit.md†L1-L200】 |
| Thread pool + buffer + query port | MVP2 | MVP2 | Implementation differs (pthread vs std::thread).【F:c/versions/mvp2-c-thread-pool/commit.md†L1-L200】【F:cpp/versions/mvp2-cpp-modern-threading/commit.md†L1-L200】 |
| Enhanced query language | MVP3 | MVP3 | Both add parser; C uses POSIX regex, C++ uses std::regex.【F:c/versions/mvp3-enhanced-query/commit.md†L1-L200】【F:cpp/versions/mvp3-enhanced-query/commit.md†L1-L200】 |
| Persistence | MVP4 | MVP4 | Async writer threads, CLI flags aligned.【F:c/versions/mvp4-persistence/commit.md†L1-L200】【F:cpp/versions/mvp4-persistence/commit.md†L1-L200】 |
| Hardening / security | MVP5 | MVP6 (IRC) | C adds mutexes/safe parsing; C++ introduces IRC. Need equivalent hardening step before/after IRC when rebuilding.【F:c/versions/mvp5-security-fixes/commit.md†L1-L200】【F:cpp/versions/mvp5-irc/commit.md†L1-L200】 |
| IRC integration | — | MVP6 | Only C++ track; C track remains TCP/Query only.

## 3. Divergence Considerations
- **Concurrency**: C++ thread pool exposes futures; C version returns void. Rebuild should maintain simple C API while optionally enhancing C++ ergonomics.
- **Regex Flavor**: POSIX vs ECMAScript differences may cause query result discrepancies. Document in user guide and potentially normalize via compatibility layer.
- **Persistence Cleanup**: C implementation left TODO for pruning rotated files; the rebuild should implement cleanup for both tracks to keep parity with C++.
- **IRC Metadata**: C++ log entries include level/source metadata not present in C logs. When generating logs from C track, we may synthesize metadata (e.g., default INFO) if cross-streaming features are added later.

## 4. Migration Steps
1. **Document (Step A)** – Completed via this spec set; legacy code still available for reference.
2. **Initialize Rebuild** – Create `work/` scaffolding with CMake, modules, and placeholder tests.
3. **Implement MVP1** – Basic sockets and CLI for both tracks, using SEQ numbering starting at SEQ0000.
4. **Iterate MVP2–MVP5/6** – After each stage: run tests, tag sequences, capture snapshot.
5. **Retire Legacy** – Remove old `c/` and `cpp/` directories after Step A (mandatory).

## 5. Testing Parity
- Legacy tests (Python) covered concurrency, persistence, security, and IRC. Rebuild must reintroduce equivalent tests under `tests/` with `ctest` integration.
- Ensure new tests map to SEQ annotations (list test names in header comment change block).

## 6. Documentation Migration
- Legacy READMEs provided extensive marketing copy; new docs will focus on practical operations, API references, and diagrams.
- Sequence numbering will be normalized to `SEQxxxx` strings with zero padding, replacing `[SEQUENCE: n]` comments.

