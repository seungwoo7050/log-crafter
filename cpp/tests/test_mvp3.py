#!/usr/bin/env python3
"""
Test script for LogCrafter-CPP MVP3 Enhanced Query Features
"""

import socket
import time
import threading
import sys
from datetime import datetime

PORT_LOG = 9999
PORT_QUERY = 9998
HOST = 'localhost'

def send_log(message):
    """Send a log message to the server"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((HOST, PORT_LOG))
        sock.sendall(f"{message}\n".encode())
        time.sleep(0.1)

def query_server(command):
    """Send a query to the server and get response"""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.connect((HOST, PORT_QUERY))
        sock.sendall(f"{command}\n".encode())
        response = sock.recv(4096).decode()
        return response

def test_mvp3_features():
    """Test MVP3 enhanced query features"""
    print("=== LogCrafter-CPP MVP3 Feature Test ===\n")
    
    # Send test logs
    print("1. Sending test logs...")
    logs = [
        "System started successfully",
        "ERROR: Failed to connect to database",
        "WARNING: Low memory detected",
        "ERROR: Connection timeout after 30s",
        "INFO: User login successful",
        "CRITICAL: System failure detected",
        "ERROR: Database connection failed",
        "DEBUG: Processing request ID 12345",
        "WARNING: High CPU usage: 85%",
        "ERROR: Authentication failed for user admin"
    ]
    
    for log in logs:
        send_log(log)
        print(f"   Sent: {log}")
    
    time.sleep(1)
    
    # Test HELP command
    print("\n2. Testing HELP command:")
    response = query_server("HELP")
    print(f"   Response:\n{response}")
    
    # Test simple keyword search (backward compatibility)
    print("\n3. Testing simple keyword search:")
    response = query_server("QUERY keyword=ERROR")
    print(f"   QUERY keyword=ERROR")
    print(f"   Response: {response}")
    
    # Test multiple keywords with AND
    print("\n4. Testing multiple keywords with AND:")
    response = query_server("QUERY keywords=ERROR,failed operator=AND")
    print(f"   QUERY keywords=ERROR,failed operator=AND")
    print(f"   Response: {response}")
    
    # Test multiple keywords with OR
    print("\n5. Testing multiple keywords with OR:")
    response = query_server("QUERY keywords=WARNING,CRITICAL operator=OR")
    print(f"   QUERY keywords=WARNING,CRITICAL operator=OR")
    print(f"   Response: {response}")
    
    # Test regex pattern
    print("\n6. Testing regex pattern:")
    response = query_server("QUERY regex=ERROR.*timeout")
    print(f"   QUERY regex=ERROR.*timeout")
    print(f"   Response: {response}")
    
    # Test complex regex
    print("\n7. Testing complex regex:")
    response = query_server("QUERY regex=.*fail(ed|ure).*")
    print(f"   QUERY regex=.*fail(ed|ure).*")
    print(f"   Response: {response}")
    
    # Test time-based filtering (current time)
    print("\n8. Testing time-based filtering:")
    current_time = int(time.time())
    past_time = current_time - 3600  # 1 hour ago
    response = query_server(f"QUERY time_from={past_time} time_to={current_time}")
    print(f"   QUERY time_from={past_time} time_to={current_time}")
    print(f"   Response: {response}")
    
    # Test combined query
    print("\n9. Testing combined query (keywords + regex):")
    response = query_server("QUERY keywords=ERROR,failed operator=AND regex=.*database.*")
    print(f"   QUERY keywords=ERROR,failed operator=AND regex=.*database.*")
    print(f"   Response: {response}")
    
    # Test all parameters combined
    print("\n10. Testing all parameters combined:")
    response = query_server(f"QUERY keywords=ERROR operator=AND regex=.*failed.* time_from={past_time}")
    print(f"   QUERY keywords=ERROR operator=AND regex=.*failed.* time_from={past_time}")
    print(f"   Response: {response}")
    
    # Test STATS command
    print("\n11. Testing STATS command:")
    response = query_server("STATS")
    print(f"   Response: {response}")
    
    print("\n=== MVP3 Test Complete ===")

if __name__ == "__main__":
    try:
        test_mvp3_features()
    except ConnectionRefusedError:
        print("Error: Could not connect to server. Make sure LogCrafter-CPP is running.")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)