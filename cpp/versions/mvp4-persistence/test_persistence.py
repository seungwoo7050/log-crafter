#!/usr/bin/env python3
"""
Test script for LogCrafter-CPP MVP4 Persistence Features
"""

import socket
import time
import subprocess
import os
import sys
import signal

PORT_LOG = 9999
PORT_QUERY = 9998
HOST = 'localhost'
LOG_DIR = './logs'

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

def start_server(with_persistence=True):
    """Start the LogCrafter-CPP server"""
    cmd = ['./logcrafter-cpp']
    if with_persistence:
        cmd.extend(['-P', '-d', LOG_DIR, '-s', '1'])  # 1MB file size for testing
    
    print(f"Starting server: {' '.join(cmd)}")
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(2)  # Wait for server to start
    return process

def stop_server(process):
    """Stop the server gracefully"""
    process.send_signal(signal.SIGINT)
    process.wait(timeout=5)

def test_persistence():
    """Test persistence features"""
    print("=== LogCrafter-CPP MVP4 Persistence Test ===\n")
    
    # Clean up any existing logs
    if os.path.exists(LOG_DIR):
        for file in os.listdir(LOG_DIR):
            if file.endswith('.log'):
                os.remove(os.path.join(LOG_DIR, file))
    
    # Test 1: Basic persistence
    print("1. Testing basic persistence...")
    server = start_server(with_persistence=True)
    
    try:
        # Send some logs
        logs = [
            "System initialized with C++",
            "User login: admin (RAII)",
            "std::filesystem working properly",
            "ERROR: std::exception caught",
            "WARNING: High memory usage in std::vector"
        ]
        
        for log in logs:
            send_log(log)
            print(f"   Sent: {log}")
        
        time.sleep(2)  # Wait for flush
        
        # Check if current.log exists
        current_log = os.path.join(LOG_DIR, 'current.log')
        if os.path.exists(current_log):
            print(f"\n   ✓ Current log file created: {current_log}")
            with open(current_log, 'r') as f:
                lines = f.readlines()
                print(f"   ✓ {len(lines)} lines written to disk")
                
                # Verify content
                for line in lines[:3]:
                    print(f"      {line.strip()}")
        else:
            print("   ✗ Current log file not found!")
            
    finally:
        stop_server(server)
    
    print("\n2. Testing log recovery on restart...")
    
    # Start server again
    server = start_server(with_persistence=True)
    
    try:
        time.sleep(2)
        
        # Query to see if logs are in memory
        response = query_server("COUNT")
        print(f"   Count after restart: {response.strip()}")
        
        # Send more logs
        send_log("After restart: C++ persistence test 1")
        send_log("After restart: C++ persistence test 2")
        
        time.sleep(2)
        
        # Check file again
        with open(current_log, 'r') as f:
            lines = f.readlines()
            print(f"   ✓ Total lines in file: {len(lines)}")
            
    finally:
        stop_server(server)
    
    print("\n3. Testing without persistence...")
    
    # Clean up logs
    if os.path.exists(current_log):
        os.remove(current_log)
    
    server = start_server(with_persistence=False)
    
    try:
        # Send logs
        send_log("Memory only: C++ Log 1")
        send_log("Memory only: C++ Log 2")
        
        time.sleep(1)
        
        # Check that no file was created
        if not os.path.exists(current_log):
            print("   ✓ No log file created (as expected)")
        else:
            print("   ✗ Log file created when persistence disabled!")
            
        # Verify logs are in memory
        response = query_server("COUNT")
        print(f"   In-memory count: {response.strip()}")
        
    finally:
        stop_server(server)
    
    print("\n4. Testing log rotation...")
    
    server = start_server(with_persistence=True)
    
    try:
        # Send many logs to trigger rotation
        print("   Sending logs to trigger rotation...")
        for i in range(100):
            send_log(f"C++ Rotation test log entry {i:04d} - " + "x" * 1000)  # Large logs
            
        time.sleep(3)  # Wait for flush and rotation
        
        # Check for rotated files
        log_files = [f for f in os.listdir(LOG_DIR) if f.endswith('.log')]
        print(f"   ✓ Found {len(log_files)} log files:")
        for f in sorted(log_files):
            size = os.path.getsize(os.path.join(LOG_DIR, f))
            print(f"      {f} ({size} bytes)")
            
    finally:
        stop_server(server)
    
    print("\n5. Testing C++ specific features...")
    
    server = start_server(with_persistence=True)
    
    try:
        # Test with emojis and special characters (C++ handles Unicode better)
        special_logs = [
            "Unicode test: 你好世界 🌍",
            "Special chars: <>&\"'",
            "Long message: " + "A" * 2000
        ]
        
        for log in special_logs:
            send_log(log)
            
        time.sleep(2)
        
        # Verify special characters are preserved
        with open(current_log, 'r') as f:
            content = f.read()
            if "你好世界" in content and "🌍" in content:
                print("   ✓ Unicode handling works correctly")
            else:
                print("   ✗ Unicode handling failed")
                
    finally:
        stop_server(server)
    
    print("\n=== C++ Persistence Test Complete ===")

if __name__ == "__main__":
    try:
        test_persistence()
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)