#!/usr/bin/env python3
"""Debug multi-keyword search"""

import socket
import time

def send_log(message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(('localhost', 9999))
    sock.send(f"{message}\n".encode())
    sock.close()

def send_query(query):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
    sock.connect(('localhost', 9998))
    sock.send(f"{query}\n".encode())
    response = sock.recv(4096).decode()
    sock.close()
    return response

# Send test logs
logs = [
    "ERROR: Database failed",
    "WARNING: High memory",
    "ERROR: Network issue",
    "INFO: System OK"
]

for log in logs:
    send_log(log)
    time.sleep(0.1)

# Test queries
print("Query: keyword=ERROR")
print(send_query("QUERY keyword=ERROR"))

print("\nQuery: keywords=ERROR,WARNING operator=OR")
print(send_query("QUERY keywords=ERROR,WARNING operator=OR"))

print("\nQuery: keywords=ERROR,Database operator=AND")
print(send_query("QUERY keywords=ERROR,Database operator=AND"))