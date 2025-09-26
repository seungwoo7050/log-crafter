#!/usr/bin/env python3
"""
Test script for LogCrafter IRC integration
Tests basic IRC functionality and log streaming
"""

import socket
import time
import threading
import sys

class IRCTestClient:
    def __init__(self, server='localhost', port=6667, nick='testbot'):
        self.server = server
        self.port = port
        self.nick = nick
        self.socket = None
        self.running = False
        
    def connect(self):
        """Connect to IRC server"""
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.socket.connect((self.server, self.port))
        self.running = True
        
        # Send registration
        self.send(f"NICK {self.nick}")
        self.send(f"USER {self.nick} 0 * :Test Bot")
        
    def send(self, message):
        """Send message to server"""
        if not message.endswith('\r\n'):
            message += '\r\n'
        self.socket.send(message.encode('utf-8'))
        print(f">>> {message.strip()}")
        
    def receive(self):
        """Receive message from server"""
        data = self.socket.recv(4096).decode('utf-8')
        return data
        
    def join(self, channel):
        """Join a channel"""
        self.send(f"JOIN {channel}")
        
    def privmsg(self, target, message):
        """Send PRIVMSG"""
        self.send(f"PRIVMSG {target} :{message}")
        
    def quit(self, message="Test complete"):
        """Quit from server"""
        self.send(f"QUIT :{message}")
        self.running = False
        self.socket.close()
        
    def read_loop(self):
        """Read messages from server"""
        while self.running:
            try:
                data = self.receive()
                for line in data.split('\r\n'):
                    if line:
                        print(f"<<< {line}")
                        
                        # Handle PING
                        if line.startswith('PING'):
                            self.send(f"PONG {line.split()[1]}")
            except:
                break

def test_irc_server():
    """Test IRC server functionality"""
    print("=== LogCrafter IRC Test ===\n")
    
    # Create test client
    client = IRCTestClient()
    
    try:
        # Connect to server
        print("1. Connecting to IRC server...")
        client.connect()
        
        # Start read thread
        read_thread = threading.Thread(target=client.read_loop)
        read_thread.daemon = True
        read_thread.start()
        
        # Wait for welcome
        time.sleep(1)
        
        # Join log channels
        print("\n2. Joining log channels...")
        client.join("#logs-all")
        time.sleep(0.5)
        client.join("#logs-error")
        time.sleep(0.5)
        
        # Send test message
        print("\n3. Sending test message...")
        client.privmsg("#logs-all", "Hello from test bot!")
        time.sleep(0.5)
        
        # Test log query command
        print("\n4. Testing log query...")
        client.privmsg("#logs-all", "!query COUNT")
        time.sleep(1)
        
        # List channels
        print("\n5. Listing channels...")
        client.send("LIST")
        time.sleep(1)
        
        # Get channel info
        print("\n6. Getting channel info...")
        client.send("NAMES #logs-error")
        time.sleep(0.5)
        
        print("\n7. Waiting for log messages...")
        print("(Send logs to port 9999 to see them appear in IRC channels)")
        time.sleep(5)
        
    except Exception as e:
        print(f"\nError: {e}")
    finally:
        # Disconnect
        print("\n8. Disconnecting...")
        client.quit()
        time.sleep(0.5)
        
    print("\n=== Test Complete ===")

def send_test_logs():
    """Send test logs to LogCrafter"""
    print("\nSending test logs to LogCrafter...")
    
    try:
        log_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        log_socket.connect(('localhost', 9999))
        
        test_logs = [
            "ERROR: Database connection failed",
            "WARNING: High memory usage detected",
            "INFO: Server started successfully",
            "DEBUG: Processing request ID 12345",
            "ERROR: Authentication failed for user admin",
            "INFO: Backup completed successfully"
        ]
        
        for log in test_logs:
            log_socket.send((log + '\n').encode('utf-8'))
            print(f"Sent: {log}")
            time.sleep(0.2)
            
        log_socket.close()
        print("Test logs sent successfully")
        
    except Exception as e:
        print(f"Failed to send logs: {e}")

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--send-logs":
        send_test_logs()
    else:
        # Start log sender in background
        log_thread = threading.Thread(target=send_test_logs)
        log_thread.daemon = True
        log_thread.start()
        
        # Give server time to start
        time.sleep(2)
        
        # Run IRC tests
        test_irc_server()