# LogCrafter-C

A high-performance TCP log collection server written in C with disk persistence support.

## Overview
LogCrafter-C is a production-ready log collection server that accepts log messages via TCP, stores them in memory with optional disk persistence, and provides a powerful query interface for searching and filtering logs.

## Features
- **TCP Socket Server**: Multi-client support on configurable port
- **Thread Pool Architecture**: Efficient handling of concurrent connections
- **In-Memory Circular Buffer**: Fast access with configurable size (10,000 logs)
- **Disk Persistence**: Optional asynchronous writing with file rotation
- **Advanced Query Interface**: Multiple query commands with regex support
- **Time-based Filtering**: Query logs within specific time ranges
- **Pattern Matching**: POSIX regex support for flexible searches
- **Statistics**: Track server performance and persistence metrics

## Architecture
```
┌─────────────────┐     ┌─────────────────┐
│   Log Clients   │     │  Query Clients  │
│   (Port 9999)   │     │   (Port 9998)   │
└────────┬────────┘     └────────┬────────┘
         │                       │
    ┌────▼────────────────────────▼────┐
    │          Thread Pool             │
    │    (Configurable Workers)        │
    └────┬─────────────────────┬───────┘
         │                     │
    ┌────▼──────┐         ┌───▼──────┐
    │  Logger   │         │  Query   │
    │  Module   │         │ Handler  │
    └────┬──────┘         └───┬──────┘
         │                     │
    ┌────▼─────────────────────▼────┐
    │     Circular Buffer           │
    │    (In-Memory Storage)        │
    └────────────┬──────────────────┘
                 │
    ┌────────────▼──────────────┐
    │   Persistence Manager     │
    │  (Optional Disk Write)    │
    └───────────────────────────┘
```

## Building

### Prerequisites
- GCC or Clang compiler with C11 support
- CMake 3.10 or higher
- POSIX-compliant system (Linux, macOS, Unix)
- pthread library

### Build Steps
```bash
git clone <repository>
cd LogCrafter-C
mkdir build && cd build
cmake ..
make
```

### Build Options
```bash
# Debug build
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Release build with optimizations
cmake -DCMAKE_BUILD_TYPE=Release ..
```

## Usage

### Starting the Server
```bash
# Basic usage (memory only)
./logcrafter-c

# With custom port
./logcrafter-c -p 8080

# With persistence enabled
./logcrafter-c -P -d /var/log/logcrafter -s 50

# Show all options
./logcrafter-c -h
```

### Command Line Options
- `-p PORT`: Set server port (default: 9999)
- `-P`: Enable persistence to disk
- `-d DIR`: Set log directory (default: ./logs)
- `-s SIZE`: Set max file size in MB (default: 10)
- `-h`: Show help message

### Sending Logs
```bash
# Using netcat
echo "System started successfully" | nc localhost 9999
echo "User login: john_doe" | nc localhost 9999
echo "ERROR: Database connection failed" | nc localhost 9999

# Using telnet
telnet localhost 9999
> Processing request ID: 12345
> Warning: High memory usage detected
```

### Query Interface
Connect to port 9998 for queries:

```bash
# Count total logs
echo "COUNT" | nc localhost 9998

# Search with case-insensitive match
echo "SEARCH error" | nc localhost 9998

# Filter by time range
echo "FILTER 2024-01-24 10:00:00 2024-01-24 11:00:00" | nc localhost 9998

# Regex pattern matching
echo "REGEX [Ee]rror.*failed" | nc localhost 9998

# Get recent logs
echo "RECENT 50" | nc localhost 9998

# Clear all logs
echo "CLEAR" | nc localhost 9998
```

## Query Commands

| Command | Description | Example |
|---------|-------------|---------|
| COUNT | Get total number of logs | `COUNT` |
| SEARCH | Case-insensitive substring search | `SEARCH error` |
| FILTER | Time range filter | `FILTER 2024-01-24 10:00:00 2024-01-24 11:00:00` |
| REGEX | POSIX regex pattern matching | `REGEX ^ERROR:.*timeout$` |
| RECENT | Get N most recent logs | `RECENT 100` |
| CLEAR | Clear all logs from memory | `CLEAR` |

## Persistence

### File Structure
```
logs/
├── current.log          # Active log file
├── 2024-01-24-143022.log   # Rotated log file
├── 2024-01-24-153045.log   # Rotated log file
└── ...
```

### Log Format
```
[2024-01-24 15:30:45] System initialized
[2024-01-24 15:30:46] User login: admin
[2024-01-24 15:30:47] ERROR: Connection timeout
```

### Rotation Policy
- Files rotate when reaching size limit
- Old files cleaned up after max_files limit
- Asynchronous writing minimizes performance impact

## Performance

### Benchmarks
- **Throughput**: ~50,000 logs/second (memory only)
- **Latency**: <1ms average response time
- **Concurrency**: Handles 100+ simultaneous clients
- **Memory**: ~100MB for 10,000 logs

### Optimization Tips
1. Use Release build for production
2. Adjust thread pool size based on CPU cores
3. Enable persistence only when needed
4. Use time filters to limit query results

## Testing

### Unit Tests
```bash
cd build
make test
```

### Integration Tests
```bash
cd tests
python3 test_all.py
```

### Test Scripts
- `test_basic.py`: Basic functionality
- `test_concurrent.py`: Concurrent client handling
- `test_query.py`: Query interface testing
- `test_persistence.py`: Persistence features
- `test_stress.py`: Load and stress testing

## Development

### Project Structure
```
LogCrafter-C/
├── include/
│   ├── server.h
│   ├── logger.h
│   ├── thread_pool.h
│   ├── circular_buffer.h
│   ├── query_handler.h
│   └── persistence.h
├── src/
│   ├── main.c
│   ├── server.c
│   ├── logger.c
│   ├── thread_pool.c
│   ├── circular_buffer.c
│   ├── query_handler.c
│   └── persistence.c
├── tests/
│   └── test_*.py
├── CMakeLists.txt
└── README.md
```

### Code Style
- POSIX C11 standard
- Clear function and variable names
- Comprehensive error handling
- Thread-safe implementations

## Troubleshooting

### Common Issues

1. **Port Already in Use**
   ```bash
   Error: Address already in use
   Solution: Change port with -p option or kill existing process
   ```

2. **Permission Denied**
   ```bash
   Error: Failed to create log directory
   Solution: Check directory permissions or run with appropriate rights
   ```

3. **High Memory Usage**
   ```bash
   Issue: Server consuming too much memory
   Solution: Logs are stored in memory; use CLEAR command periodically
   ```

## Security Considerations

- No authentication (add reverse proxy for production)
- Input validation to prevent buffer overflows
- Safe string handling with bounds checking
- Separate ports for logs and queries

## License
MIT License - See LICENSE file for details

## Contributing
1. Fork the repository
2. Create feature branch
3. Add tests for new features
4. Ensure all tests pass
5. Submit pull request

## Version History
- **MVP1** (v0.1.0): Basic TCP server with threading
- **MVP2** (v0.2.0): Circular buffer and query interface
- **MVP3** (v0.3.0): Regex support and time filtering
- **MVP4** (v1.0.0): Disk persistence with rotation