"""
Sequence: SEQ0085
Track: Shared
MVP: Step C
Change: Implement Step C spec scenarios covering protocol happy paths, invalid inputs,
        partial I/O, idle timeouts, and SIGINT shutdown behaviour for both tracks.
Tests: spec_protocol_happy_path, spec_invalid_inputs, spec_partial_io, spec_timeouts, spec_sigint_shutdown
"""

from __future__ import annotations

import argparse
import signal
import socket
import time
from collections.abc import Iterable

from tests.common.runtime import ServerProcess, binary_path


def _read_until(sock: socket.socket, substrings: Iterable[str], timeout: float = 3.0) -> str:
    sock.settimeout(0.5)
    buffer = ""
    deadline = time.monotonic() + timeout
    substrings = tuple(substrings)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(1024)
        except socket.timeout:
            continue
        if not chunk:
            break
        buffer += chunk.decode(errors="ignore")
        if all(sub in buffer for sub in substrings):
            return buffer
    raise AssertionError(f"Timed out waiting for substrings {substrings!r}; captured={buffer!r}")


def _read_all(sock: socket.socket, timeout: float = 3.0) -> str:
    sock.settimeout(0.5)
    chunks: list[str] = []
    deadline = time.monotonic() + timeout
    while True:
        if time.monotonic() > deadline:
            break
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            break
        chunks.append(chunk.decode(errors="ignore"))
    return "".join(chunks)


def _send_log_line(port: int, message: str, chunks: Iterable[bytes] | None = None) -> None:
    with socket.create_connection(("127.0.0.1", port), timeout=1.0) as sock:
        _read_until(sock, ("LogCrafter",))
        if chunks is None:
            chunks = [message.encode() + b"\n"]
        for piece in chunks:
            sock.sendall(piece)
            time.sleep(0.05)
        sock.shutdown(socket.SHUT_WR)


def _query_command(port: int, command: str) -> str:
    with socket.create_connection(("127.0.0.1", port), timeout=1.0) as sock:
        _read_until(sock, ("Commands",))
        sock.sendall((command + "\n").encode())
        sock.shutdown(socket.SHUT_WR)
        return _read_all(sock)


def spec_protocol_happy_path() -> None:
    """Sequence: SEQ0086. Validates MVP1 protocol handling (SEQ0001–SEQ0003, SEQ0031–SEQ0033)."""

    c_binary = binary_path("c")
    message = "spec-happy-c"
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        _send_log_line(9999, message)
        count_response = _query_command(9998, "COUNT")
        assert "COUNT:" in count_response
        count_value = int(count_response.split(":", 1)[1].strip())
        assert count_value >= 1
        query_response = _query_command(9998, f"QUERY keyword={message}")
        assert "FOUND:" in query_response
        assert message in query_response

    cpp_binary = binary_path("cpp")
    cpp_log = 15100
    cpp_query = 15101
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        message_cpp = "spec-happy-cpp"
        _send_log_line(cpp_log, message_cpp)
        count_response = _query_command(cpp_query, "COUNT")
        assert "COUNT:" in count_response
        cpp_count = int(count_response.split(":", 1)[1].strip())
        assert cpp_count >= 1
        query_response = _query_command(cpp_query, f"QUERY keyword={message_cpp}")
        assert "FOUND:" in query_response
        assert message_cpp in query_response


def spec_invalid_inputs() -> None:
    """Sequence: SEQ0087. Exercises error paths from SEQ0011–SEQ0016 and SEQ0041–SEQ0046."""

    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        error_unknown = _query_command(9998, "NOPE")
        assert "ERROR:" in error_unknown
        error_invalid = _query_command(9998, "QUERY keywords=alpha operator=NOTREAL")
        assert "ERROR:" in error_invalid

    cpp_binary = binary_path("cpp")
    cpp_log = 15110
    cpp_query = 15111
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        error_unknown = _query_command(cpp_query, "NOPE")
        assert "ERROR:" in error_unknown
        error_invalid = _query_command(cpp_query, "QUERY keywords=beta operator=SOMETHING")
        assert "ERROR:" in error_invalid


def spec_partial_io() -> None:
    """Sequence: SEQ0088. Validates partial I/O handling from SEQ0024–SEQ0029 and SEQ0034–SEQ0040."""

    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        fragments = [b"partial-", b"io", b"-c\n"]
        _send_log_line(9999, "partial-io-c", fragments)
        with socket.create_connection(("127.0.0.1", 9998), timeout=1.0) as sock:
            _read_until(sock, ("Commands",))
            for chunk in ["Q", "UERY keyword=partial-io-c", "\n"]:
                sock.sendall(chunk.encode())
                time.sleep(0.05)
            sock.shutdown(socket.SHUT_WR)
            response = _read_all(sock)
        assert "FOUND:" in response
        assert "partial-io-c" in response

    cpp_binary = binary_path("cpp")
    cpp_log = 15120
    cpp_query = 15121
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        fragments = [b"partial-", b"io", b"-cpp\n"]
        _send_log_line(cpp_log, "partial-io-cpp", fragments)
        with socket.create_connection(("127.0.0.1", cpp_query), timeout=1.0) as sock:
            _read_until(sock, ("Commands",))
            for chunk in ["Q", "UERY keyword=partial-io-cpp", "\n"]:
                sock.sendall(chunk.encode())
                time.sleep(0.05)
            sock.shutdown(socket.SHUT_WR)
            response = _read_all(sock)
        assert "FOUND:" in response
        assert "partial-io-cpp" in response


def spec_timeouts() -> None:
    """Sequence: SEQ0089. Confirms select-loop timeout responsiveness from SEQ0004–SEQ0010 and SEQ0034–SEQ0040."""

    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        idle_sock = socket.create_connection(("127.0.0.1", 9999), timeout=1.0)
        try:
            _read_until(idle_sock, ("LogCrafter",))
            time.sleep(1.2)
            count_response = _query_command(9998, "COUNT")
            assert "COUNT:" in count_response
        finally:
            idle_sock.close()

    cpp_binary = binary_path("cpp")
    cpp_log = 15130
    cpp_query = 15131
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        idle_sock = socket.create_connection(("127.0.0.1", cpp_log), timeout=1.0)
        try:
            _read_until(idle_sock, ("LogCrafter",))
            time.sleep(1.2)
            count_response = _query_command(cpp_query, "COUNT")
            assert "COUNT:" in count_response
        finally:
            idle_sock.close()


def spec_sigint_shutdown() -> None:
    """Sequence: SEQ0090. Verifies clean SIGINT shutdown per SEQ0017–SEQ0023 and SEQ0065–SEQ0075."""

    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        exit_code = server.terminate(signal.SIGINT)
        assert exit_code == 0
        assert "shutdown complete" in server.stderr

    cpp_binary = binary_path("cpp")
    cpp_log = 15140
    cpp_query = 15141
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        exit_code = server.terminate(signal.SIGINT)
        assert exit_code == 0
        assert "server initialized" in server.stderr


SPEC_CASES = {
    "spec_protocol_happy_path": spec_protocol_happy_path,
    "spec_invalid_inputs": spec_invalid_inputs,
    "spec_partial_io": spec_partial_io,
    "spec_timeouts": spec_timeouts,
    "spec_sigint_shutdown": spec_sigint_shutdown,
}


def main() -> None:
    parser = argparse.ArgumentParser(description="Run LogCrafter spec scenarios")
    parser.add_argument("case", choices=sorted(SPEC_CASES.keys()))
    args = parser.parse_args()
    SPEC_CASES[args.case]()


if __name__ == "__main__":
    main()
