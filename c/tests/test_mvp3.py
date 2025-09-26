#!/usr/bin/env python3
"""
LogCrafter-C MVP3 Test Suite
Tests enhanced query capabilities: regex, time filtering, multi-keyword search
"""

import socket
import time
import threading
import subprocess
import random
import datetime
import re

# Server configuration
LOG_PORT = 9999
QUERY_PORT = 9998
HOST = 'localhost'

def send_log(message):
    """Send a log message to the server"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, LOG_PORT))
        sock.send(f"{message}\n".encode())
        sock.close()
    except Exception as e:
        print(f"Error sending log: {e}")

def send_query(query):
    """Send a query and get response"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((HOST, QUERY_PORT))
        sock.send(f"{query}\n".encode())
        response = sock.recv(4096).decode()
        sock.close()
        return response
    except Exception as e:
        print(f"Error sending query: {e}")
        return None

def test_help_command():
    """Test HELP command"""
    print("\n=== Testing HELP Command ===")
    response = send_query("HELP")
    print(f"Response:\n{response}")
    return "LogCrafter Query Interface - MVP3" in response

def test_simple_keyword_search():
    """Test backward-compatible simple keyword search"""
    print("\n=== Testing Simple Keyword Search ===")
    
    # Send test logs
    test_logs = [
        "INFO: Application started successfully",
        "ERROR: Failed to connect to database",
        "WARNING: Cache memory usage high",
        "ERROR: Network timeout occurred",
        "INFO: User login successful"
    ]
    
    for log in test_logs:
        send_log(log)
        time.sleep(0.1)
    
    # Search for ERROR
    response = send_query("QUERY keyword=ERROR")
    print(f"Search for 'ERROR':\n{response}")
    
    return "FOUND: 2 matches" in response

def test_regex_search():
    """Test regex pattern matching"""
    print("\n=== Testing Regex Search ===")
    
    # Send logs with patterns
    test_logs = [
        "ERROR: Connection timeout after 30s",
        "ERROR: Read timeout after 60s", 
        "ERROR: Write failed with code 500",
        "WARNING: Slow response time 5000ms",
        "ERROR: Network timeout detected"
    ]
    
    for log in test_logs:
        send_log(log)
        time.sleep(0.1)
    
    # Search with regex
    response = send_query("QUERY regex=ERROR.*timeout")
    print(f"Regex search 'ERROR.*timeout':\n{response}")
    
    return "FOUND: 3 matches" in response

def test_time_filtering():
    """Test time range filtering"""
    print("\n=== Testing Time Filtering ===")
    
    # Get current timestamp
    current_time = int(time.time())
    
    # Send logs with some delay
    send_log("LOG: Before time range")
    time.sleep(2)
    
    time_from = int(time.time())
    send_log("LOG: Within time range 1")
    time.sleep(0.5)
    send_log("LOG: Within time range 2")
    time.sleep(0.5)
    send_log("LOG: Within time range 3")
    time_to = int(time.time()) + 1
    
    time.sleep(2)
    send_log("LOG: After time range")
    
    # Query with time range
    response = send_query(f"QUERY time_from={time_from} time_to={time_to}")
    print(f"Time range query:\n{response}")
    
    return "FOUND: 3 matches" in response

def test_multi_keyword_search():
    """Test multiple keyword search with AND/OR operators"""
    print("\n=== Testing Multi-Keyword Search ===")
    
    # Send test logs
    test_logs = [
        "ERROR: Database connection failed",
        "WARNING: High memory usage detected",
        "ERROR: High CPU usage detected",
        "INFO: System running normally",
        "WARNING: Database slow response"
    ]
    
    for log in test_logs:
        send_log(log)
        time.sleep(0.1)
    
    # Test OR operator
    response = send_query("QUERY keywords=ERROR,WARNING operator=OR")
    print(f"OR search (ERROR,WARNING):\n{response}")
    or_matches = int(re.search(r'FOUND: (\d+)', response).group(1)) if response else 0
    
    # Test AND operator
    response = send_query("QUERY keywords=ERROR,CPU operator=AND")
    print(f"AND search (ERROR,CPU):\n{response}")
    and_matches = int(re.search(r'FOUND: (\d+)', response).group(1)) if response else 0
    
    return or_matches == 4 and and_matches == 1

def test_complex_query():
    """Test complex query with multiple parameters"""
    print("\n=== Testing Complex Query ===")
    
    # Send varied logs
    test_logs = [
        "ERROR: Authentication failed for user admin",
        "WARNING: Login attempt failed",
        "ERROR: Permission denied for user guest",
        "INFO: User logged in successfully",
        "ERROR: Session timeout for inactive user"
    ]
    
    for log in test_logs:
        send_log(log)
        time.sleep(0.1)
    
    # Complex query: ERROR logs with 'user' keyword
    response = send_query("QUERY keywords=ERROR,user operator=AND")
    print(f"Complex query (ERROR AND user):\n{response}")
    
    return "FOUND: 3 matches" in response

def test_stats_and_count():
    """Test STATS and COUNT commands still work"""
    print("\n=== Testing STATS and COUNT ===")
    
    # Get stats
    stats_response = send_query("STATS")
    print(f"STATS:\n{stats_response}")
    
    # Get count
    count_response = send_query("COUNT")
    print(f"COUNT:\n{count_response}")
    
    return "STATS:" in stats_response and "COUNT:" in count_response

def run_tests():
    """Run all MVP3 tests"""
    print("LogCrafter-C MVP3 Test Suite")
    print("============================")
    
    # Wait for server to be ready
    time.sleep(2)
    
    # Run tests
    tests = [
        ("HELP Command", test_help_command),
        ("Simple Keyword Search", test_simple_keyword_search),
        ("Regex Search", test_regex_search),
        ("Time Filtering", test_time_filtering),
        ("Multi-Keyword Search", test_multi_keyword_search),
        ("Complex Query", test_complex_query),
        ("Stats and Count", test_stats_and_count)
    ]
    
    passed = 0
    failed = 0
    
    for test_name, test_func in tests:
        try:
            if test_func():
                print(f"✓ {test_name} PASSED")
                passed += 1
            else:
                print(f"✗ {test_name} FAILED")
                failed += 1
        except Exception as e:
            print(f"✗ {test_name} FAILED with exception: {e}")
            failed += 1
        
        time.sleep(1)  # Give server time between tests
    
    print(f"\n=== Test Summary ===")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Total: {passed + failed}")
    
    return failed == 0

if __name__ == "__main__":
    # Note: Server should be started separately
    print("Make sure LogCrafter-C server is running on ports 9999/9998")
    print("Starting tests in 3 seconds...")
    time.sleep(3)
    
    success = run_tests()
    exit(0 if success else 1)