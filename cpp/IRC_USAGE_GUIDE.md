# IRC Integration Usage Guide

## Overview

LogCrafter's IRC integration allows you to monitor logs in real-time using any IRC client. This guide covers setup, usage, and advanced features.

## Quick Start

### 1. Start LogCrafter with IRC
```bash
# Default IRC port (6667)
./logcrafter-cpp -i

# Custom IRC port
./logcrafter-cpp -I 6668

# With all features
./logcrafter-cpp -P -d ./logs -i
```

### 2. Connect with IRC Client

#### Using irssi
```bash
irssi -c localhost -p 6667
```

#### Using HexChat
1. Server → Network List → Add
2. Server: localhost/6667
3. Connect

#### Using web-based client (KiwiIRC)
```
Server: localhost
Port: 6667
Nick: your_nickname
```

### 3. Basic Commands
```irc
/nick myusername
/join #logs-all
/join #logs-error
/list
```

## Available Channels

### System Channels
- `#logs-all` - All log messages
- `#logs-error` - ERROR level logs only
- `#logs-warning` - WARNING level logs only
- `#logs-info` - INFO level logs only
- `#logs-debug` - DEBUG level logs only

### Channel Features
- Auto-created on server start
- Real-time log streaming
- Filtered by log level
- Topic shows channel purpose

## Sending Logs

### From Command Line
```bash
# Send test logs
echo "ERROR: Database connection failed" | nc localhost 9999
echo "INFO: User logged in" | nc localhost 9999
echo "WARNING: High CPU usage" | nc localhost 9999
```

### From Application
```python
import socket

def send_log(level, message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 9999))
    sock.send(f"{level}: {message}\n".encode())
    sock.close()

send_log("ERROR", "Failed to process payment")
send_log("INFO", "Order #12345 completed")
```

## IRC Commands

### Standard IRC Commands
- `/join #channel` - Join a channel
- `/part #channel` - Leave a channel
- `/list` - List all channels
- `/names #channel` - List users in channel
- `/topic #channel` - View channel topic
- `/quit` - Disconnect from server

### LogCrafter Custom Commands

Send these in any log channel:

#### Query Logs
```irc
!query COUNT
!query COUNT level:ERROR
!query SEARCH database
!query LAST 10
```

#### Filter Management
```irc
!filter add keyword:security
!filter remove keyword:security
!filter list
```

#### Stream Control
```irc
!stream on
!stream off
!stream status
```

## Advanced Usage

### Creating Custom Channels

While custom channels aren't persistent, you can create them for temporary filtering:

```irc
/join #logs-custom
!filter add level:ERROR keyword:database
```

### Multi-Client Monitoring

Connect multiple IRC clients for different views:
- Client 1: Monitor #logs-error for critical issues
- Client 2: Monitor #logs-info for general activity
- Client 3: Use #logs-all with custom filters

### Integration with IRC Bots

Create bots to automate responses:

```python
import irc.bot

class LogMonitorBot(irc.bot.SingleServerIRCBot):
    def __init__(self):
        super().__init__([("localhost", 6667)], "logbot", "Log Monitor Bot")
        
    def on_welcome(self, connection, event):
        connection.join("#logs-error")
        
    def on_pubmsg(self, connection, event):
        message = event.arguments[0]
        if "CRITICAL" in message:
            # Send alert
            self.send_alert(message)
```

### Log Formatting

Logs appear in IRC as:
```
<LogBot> [2024-01-25 10:30:45] ERROR: Database connection failed
<LogBot> [2024-01-25 10:30:46] INFO: Retry attempt 1 of 3
```

## Performance Tips

### 1. Channel Selection
- Join only needed channels to reduce traffic
- Use level-specific channels for filtering

### 2. Client Configuration
- Enable message buffering in your IRC client
- Set appropriate scrollback limits
- Use client-side filtering for additional control

### 3. Network Optimization
- Run LogCrafter and IRC client on same machine when possible
- Use localhost connection to minimize latency

## Troubleshooting

### Connection Issues
```
Problem: Cannot connect to IRC server
Solution: 
1. Verify LogCrafter is running with -i flag
2. Check port availability: netstat -an | grep 6667
3. Try telnet localhost 6667
```

### Missing Logs
```
Problem: Logs not appearing in IRC channels
Solution:
1. Verify logs are being sent to port 9999
2. Check log level matches channel filter
3. Ensure IRC client is in the channel
```

### Performance Problems
```
Problem: IRC client lagging with high log volume
Solution:
1. Use level-specific channels instead of #logs-all
2. Implement client-side rate limiting
3. Increase IRC client buffer size
```

## Security Considerations

1. **Access Control**: IRC server accepts all connections on localhost
2. **Message Privacy**: All channel members see all logs
3. **Rate Limiting**: No built-in flood protection
4. **Authentication**: No password required by default

For production use, consider:
- Binding to specific interfaces only
- Implementing authentication
- Adding SSL/TLS support
- Rate limiting per client

## Examples

### Monitor Errors Only
```bash
# Terminal 1: Start server
./logcrafter-cpp -i

# Terminal 2: Connect IRC
irssi -c localhost
/nick errormonitor
/join #logs-error

# Terminal 3: Generate test errors
for i in {1..10}; do
    echo "ERROR: Test error $i" | nc localhost 9999
    sleep 1
done
```

### Real-time Debugging
```bash
# In IRC client
/join #logs-debug
!filter add keyword:UserID:12345

# Now only logs containing "UserID:12345" appear
```

### Automated Alerting
```python
# log_alerter.py
import socket
import re

irc_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
irc_sock.connect(('localhost', 6667))
irc_sock.send(b"NICK alertbot\r\n")
irc_sock.send(b"USER alertbot 0 * :Alert Bot\r\n")
irc_sock.send(b"JOIN #logs-error\r\n")

while True:
    data = irc_sock.recv(4096).decode('utf-8')
    if "ERROR:" in data and "CRITICAL" in data:
        # Send email/SMS/Slack notification
        send_alert(data)
```

## Best Practices

1. **Channel Organization**
   - Use level-based channels for different severity monitoring
   - Create naming conventions for custom channels

2. **Client Setup**
   - Configure auto-reconnect in case of disconnection
   - Set up logging in your IRC client for audit trails
   - Use screen/tmux for persistent sessions

3. **Monitoring Strategy**
   - Dedicate channels to specific subsystems
   - Implement keyword highlighting in your IRC client
   - Regular expression filters for complex patterns

4. **Integration**
   - Combine with existing monitoring tools
   - Export IRC logs for analysis
   - Create dashboards from IRC data

---

*For more information, see the main README.md and DEVELOPMENT_JOURNEY.md*