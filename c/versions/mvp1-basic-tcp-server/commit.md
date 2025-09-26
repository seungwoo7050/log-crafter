# MVP1: Basic TCP Server - Version Snapshot

**Date**: 2025-07-25
**Version**: MVP1 Complete

## Changes Summary

### Initial Implementation
- Created basic TCP server using select() for I/O multiplexing
- Implemented multi-client connection handling (up to 1024 clients)
- Added graceful shutdown with SIGINT handling
- Logs received from clients are printed to stdout

### Key Components
1. **server.h**: Core server structure and function declarations
2. **server.c**: Main server implementation with select() loop
3. **main.c**: Entry point with command-line argument parsing
4. **Makefile**: Build configuration with strict compiler flags

### Technical Decisions
- **Why select()?** Simpler than epoll/kqueue for MVP, portable across POSIX systems
- **Fixed buffer size (4KB)**: Reasonable for log messages, simple memory management
- **Thread safety**: Using volatile sig_atomic_t for signal-safe shutdown flag
- **Error handling**: Comprehensive error checking on all system calls

### Testing
- Successfully tested with up to 50 concurrent clients
- Python test client created for automated testing
- No memory leaks or crashes observed

### Known Limitations
- select() has O(n) complexity, inefficient for >1000 connections
- Fixed buffer per connection wastes memory
- No persistent storage (logs only to stdout)
- Single-threaded, all I/O on main thread

### Next Steps (MVP2)
- Implement thread pool for better concurrency
- Add in-memory log buffer with mutex synchronization
- Replace select() with epoll (Linux) or kqueue (BSD)

## Code Metrics
- Total lines of code: ~350
- Files: 4 (excluding tests)
- Compilation: Zero warnings with -Wall -Wextra -Werror -pedantic

## Sequence Documentation
The code uses [SEQUENCE: n] comments to track the logical flow:
- 1-3: Structure and function declarations
- 4-13: Server initialization and setup
- 14-19: Main event loop
- 20-27: Connection and data handling
- 28-32: Main program flow

This approach helps learners understand the execution order and design decisions.