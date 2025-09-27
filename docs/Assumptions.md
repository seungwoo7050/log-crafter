# Assumptions & Open Items

1. **Operating Environment**: Linux-like POSIX environment with epoll/select availability, pthreads, and standard build toolchain (gcc/clang, cmake).【F:AGENTS.md†L1-L120】
2. **Encoding**: Incoming log lines are UTF-8. Binary logs are out of scope; truncation retains valid UTF-8 semantics.
3. **Query Compatibility**: Existing automation expects textual responses identical to legacy output. Formatting must remain stable when rebuilding enhanced queries and HELP text.【F:c/src/query_handler.c†L1-L200】【F:cpp/src/QueryHandler.cpp†L1-L200】
4. **Persistence Directory Permissions**: Server has rights to create directories/files under specified path. Errors abort startup with clear messaging.【F:cpp/src/Persistence.cpp†L1-L200】
5. **IRC Authentication**: Legacy did not enforce password/ident. Rebuild assumes open access but reserves ability to add optional auth later.
6. **Sequence Continuity**: No gaps allowed in SEQ numbering. Initial rebuild will start at SEQ0000 and increment per feature commit.
7. **Tests**: Future tests will be rewritten in CTest-compatible C/C++ harnesses rather than Python to satisfy Step B/C deliverables.
8. **Resource Limits**: Default buffer size (10k logs) and rotation limit (10 files) remain acceptable until performance benchmarks suggest otherwise.
9. **Time Zones**: All timestamps use server local time as in legacy; UTC normalization considered but not mandated.
10. **IRC Feature Scope**: MVP6 retains only features observed in legacy source (default channels, `!query`, `!logstream`, channel stats). Advanced RFC features (DCC, SASL) remain out of scope unless requirements emerge.【F:cpp/versions/mvp5-irc/commit.md†L1-L200】

