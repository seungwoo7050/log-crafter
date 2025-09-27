"""
Sequence: SEQ0101
Track: Shared
MVP: Step C
Change: Provide a minimal load generator for integration scenarios that replays log lines
        against the active server ports.
Tests: integration_multi_client_broadcast, integration_connection_determinism
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from typing import Iterable


def _send_line(port: int, message: str) -> None:
    with socket.create_connection(("127.0.0.1", port), timeout=1.0) as sock:
        sock.settimeout(0.5)
        try:
            sock.recv(1024)
        except socket.timeout:
            pass
        sock.sendall((message + "\n").encode())
        sock.shutdown(socket.SHUT_WR)
        while True:
            try:
                if not sock.recv(1024):
                    break
            except socket.timeout:
                continue


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Replay newline-delimited log lines to the server")
    parser.add_argument("--port", type=int, required=True, help="Log ingestion port")
    parser.add_argument("--interval", type=float, default=0.05, help="Delay between messages in seconds")
    parser.add_argument("messages", nargs="+", help="Messages to send in order")
    args = parser.parse_args(list(argv) if argv is not None else None)

    for message in args.messages:
        _send_line(args.port, message)
        time.sleep(max(0.0, args.interval))
    return 0


if __name__ == "__main__":
    sys.exit(main())
