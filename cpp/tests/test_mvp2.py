#!/usr/bin/env python3
"""
Test client for LogCrafter-C MVP2
Tests thread pool functionality and query interface
"""

import socket
import sys
import time
import threading

def send_logs(client_id, host='localhost', port=9999, count=10):
    """Connect to server and send test logs"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"Client {client_id}: Connected to log server")
        
        for i in range(count):
            message = f"[Client {client_id}] Log message {i+1} - Thread pool test\n"
            sock.sendall(message.encode())
            time.sleep(0.05)  # Small delay between messages
        
        print(f"Client {client_id}: Sent {count} messages")
        sock.close()
        
    except Exception as e:
        print(f"Client {client_id}: Error - {e}")

def query_server(command, host='localhost', port=9998):
    """Send query to server and display response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        
        # Send query
        sock.sendall((command + '\n').encode())
        
        # Receive response
        response = b''
        while True:
            data = sock.recv(1024)
            if not data:
                break
            response += data
            # Simple check for end of response
            if b'\n' in data and len(data) < 100:
                break
        
        sock.close()
        return response.decode()
        
    except Exception as e:
        return f"Query error: {e}"

def test_thread_pool():
    """Test thread pool with concurrent clients"""
    print("=== Test 1: Thread Pool Concurrency ===")
    threads = []
    client_count = 10
    
    # Start multiple clients simultaneously
    for i in range(client_count):
        thread = threading.Thread(target=send_logs, args=(i+1, 'localhost', 9999, 5))
        threads.append(thread)
        thread.start()
    
    # Wait for all clients to finish
    for thread in threads:
        thread.join()
    
    print(f"All {client_count} clients completed")
    time.sleep(1)  # Let server process all logs
    print()

def test_query_interface():
    """Test query functionality"""
    print("=== Test 2: Query Interface ===")
    
    # Test STATS command
    print("Querying stats...")
    response = query_server("STATS")
    print(f"Response: {response.strip()}")
    
    # Test COUNT command
    print("\nQuerying count...")
    response = query_server("COUNT")
    print(f"Response: {response.strip()}")
    
    # Test SEARCH command
    print("\nSearching for 'message 3'...")
    response = query_server("QUERY keyword=message 3")
    print(f"Response: {response.strip()}")
    
    print()

def test_buffer_overflow():
    """Test buffer behavior when full"""
    print("=== Test 3: Buffer Overflow ===")
    print("Sending many logs to test buffer limits...")
    
    # Send a large number of logs
    send_logs(999, count=100)
    
    # Check stats to see if logs were dropped
    time.sleep(1)
    response = query_server("STATS")
    print(f"Stats after overflow test: {response.strip()}")
    print()

def test_concurrent_queries():
    """Test concurrent query handling"""
    print("=== Test 4: Concurrent Queries ===")
    
    def query_worker(query_id, command):
        response = query_server(command)
        print(f"Query {query_id} ({command}): {response.strip()}")
    
    threads = []
    queries = [
        ("STATS", 1),
        ("COUNT", 2),
        ("QUERY keyword=Client", 3),
        ("STATS", 4),
        ("COUNT", 5)
    ]
    
    for cmd, qid in queries:
        thread = threading.Thread(target=query_worker, args=(qid, cmd))
        threads.append(thread)
        thread.start()
    
    for thread in threads:
        thread.join()
    print()

if __name__ == "__main__":
    print("LogCrafter-C MVP2 Test Suite")
    print("============================")
    print("Make sure the server is running on ports 9999 (logs) and 9998 (queries)")
    print()
    
    if len(sys.argv) > 1:
        if sys.argv[1] == "pool":
            test_thread_pool()
        elif sys.argv[1] == "query":
            test_query_interface()
        elif sys.argv[1] == "overflow":
            test_buffer_overflow()
        elif sys.argv[1] == "concurrent":
            test_concurrent_queries()
        else:
            print("Usage: python3 test_mvp2.py [pool|query|overflow|concurrent]")
    else:
        # Run all tests
        test_thread_pool()
        time.sleep(1)
        test_query_interface()
        time.sleep(1)
        test_buffer_overflow()
        time.sleep(1)
        test_concurrent_queries()