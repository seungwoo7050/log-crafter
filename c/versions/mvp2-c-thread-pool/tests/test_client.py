#!/usr/bin/env python3
"""
Simple test client for LogCrafter-C server
Tests basic functionality: connection, log sending, and disconnection
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
        print(f"Client {client_id}: Connected to {host}:{port}")
        
        for i in range(count):
            message = f"Client {client_id} - Log message {i+1}\n"
            sock.sendall(message.encode())
            time.sleep(0.1)  # Small delay between messages
        
        print(f"Client {client_id}: Sent {count} messages")
        sock.close()
        print(f"Client {client_id}: Disconnected")
        
    except Exception as e:
        print(f"Client {client_id}: Error - {e}")

def test_single_client():
    """Test with a single client"""
    print("=== Test 1: Single Client ===")
    send_logs(1, count=5)
    print()

def test_multiple_clients():
    """Test with multiple concurrent clients"""
    print("=== Test 2: Multiple Clients ===")
    threads = []
    client_count = 5
    
    for i in range(client_count):
        thread = threading.Thread(target=send_logs, args=(i+1, 'localhost', 9999, 3))
        threads.append(thread)
        thread.start()
    
    for thread in threads:
        thread.join()
    
    print(f"All {client_count} clients completed")
    print()

def test_stress():
    """Stress test with many clients"""
    print("=== Test 3: Stress Test ===")
    threads = []
    client_count = 50
    
    for i in range(client_count):
        thread = threading.Thread(target=send_logs, args=(i+1, 'localhost', 9999, 2))
        threads.append(thread)
        thread.start()
        time.sleep(0.01)  # Small delay to avoid connection burst
    
    for thread in threads:
        thread.join()
    
    print(f"Stress test completed with {client_count} clients")
    print()

if __name__ == "__main__":
    if len(sys.argv) > 1:
        if sys.argv[1] == "single":
            test_single_client()
        elif sys.argv[1] == "multi":
            test_multiple_clients()
        elif sys.argv[1] == "stress":
            test_stress()
        else:
            print("Usage: python3 test_client.py [single|multi|stress]")
    else:
        # Run all tests
        test_single_client()
        time.sleep(1)
        test_multiple_clients()
        time.sleep(1)
        test_stress()