"""
Sequence: SEQ0094
Track: Shared
MVP: Step C
Change: Implement the Step C integration harness covering broadcast workflows, IRC bridging,
        and deterministic connection lifecycles across both tracks.
Tests: integration_multi_client_broadcast, integration_cpp_irc_feature, integration_connection_determinism
"""

from __future__ import annotations

import socket
import threading
import time
from dataclasses import dataclass
from typing import Callable, Iterable, List

from tests.common.runtime import ServerProcess, binary_path, wait_for_port


@dataclass
class QueryResult:
    response: str


def _read_until(sock: socket.socket, substrings: Iterable[str], timeout: float = 5.0) -> str:
    sock.settimeout(0.5)
    buffer = ""
    deadline = time.monotonic() + timeout
    substrings = tuple(substrings)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
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
    chunks: List[str] = []
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


def _send_log_line(port: int, message: str) -> None:
    with socket.create_connection(("127.0.0.1", port), timeout=1.0) as sock:
        _read_until(sock, ("LogCrafter",))
        sock.sendall((message + "\n").encode())
        sock.shutdown(socket.SHUT_WR)
        _read_all(sock)


def _query_command(port: int, command: str) -> str:
    with socket.create_connection(("127.0.0.1", port), timeout=1.0) as sock:
        _read_until(sock, ("Commands",))
        sock.sendall((command + "\n").encode())
        sock.shutdown(socket.SHUT_WR)
        return _read_all(sock)


def _multi_client_broadcast_for_track(track: str, log_port: int, query_port: int, message: str) -> None:
    binary = binary_path(track)
    extra_args: list[str] = []
    if track == "cpp":
        extra_args = [
            "--log-port",
            str(log_port),
            "--query-port",
            str(query_port),
        ]
    with ServerProcess(binary, *extra_args) as server:
        if track == "cpp":
            server.wait_ready([log_port, query_port])
        else:
            server.wait_ready([log_port, query_port])

        _send_log_line(log_port, message)

        results: list[QueryResult] = []
        exceptions: list[BaseException] = []

        def worker() -> None:
            try:
                response = _query_command(query_port, f"QUERY keyword={message}")
                results.append(QueryResult(response=response))
            except BaseException as exc:  # pragma: no cover - surfaced via assert below
                exceptions.append(exc)

        threads = [threading.Thread(target=worker) for _ in range(3)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join(timeout=5.0)
        assert all(not thread.is_alive() for thread in threads), "Query worker threads did not finish"
        if exceptions:
            raise exceptions[0]

        assert len(results) == 3
        for result in results:
            assert "FOUND:" in result.response
            assert message in result.response


def integration_multi_client_broadcast() -> None:
    """Sequence: SEQ0095. Validates concurrent query access from SEQ0004–SEQ0040."""

    _multi_client_broadcast_for_track("c", 9999, 9998, "integration-broadcast-c")
    _multi_client_broadcast_for_track("cpp", 15200, 15201, "integration-broadcast-cpp")


def _irc_register(sock: socket.socket, nick: str = "integ") -> None:
    _read_until(sock, ("LogCrafter IRC ready",))
    sock.sendall(f"NICK {nick}\r\n".encode())
    sock.sendall(f"USER {nick} 0 * :Integration Client\r\n".encode())
    _read_until(sock, (" 001 ", "JOIN :#logs-all"))


def _wait_for_irc_line(sock: socket.socket, predicate: Callable[[str], bool], timeout: float = 5.0) -> str:
    sock.settimeout(0.5)
    buffer = ""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            continue
        text = chunk.decode(errors="ignore")
        buffer += text
        lines = buffer.split("\r\n")
        buffer = lines.pop() if lines else ""
        for line in lines:
            if predicate(line):
                return line
    raise AssertionError("Timed out waiting for IRC predicate match")


def integration_cpp_irc_feature() -> None:
    """Sequence: SEQ0096. Exercises IRC bridging and commands from SEQ0054–SEQ0075."""

    log_port = 15210
    query_port = 15211
    irc_port = 15212
    binary = binary_path("cpp")
    with ServerProcess(
        binary,
        "--log-port",
        str(log_port),
        "--query-port",
        str(query_port),
        "--enable-irc",
        "--irc-port",
        str(irc_port),
        "--irc-server-name",
        "integ-irc",
    ) as server:
        server.wait_ready([log_port, query_port])
        wait_for_port(irc_port, server.process)

        with socket.create_connection(("127.0.0.1", irc_port), timeout=2.0) as irc_sock:
            _irc_register(irc_sock)
            irc_sock.sendall(b"PRIVMSG #logs-all :!help\r\n")
            help_line = _wait_for_irc_line(irc_sock, lambda line: "!query" in line)
            assert "IRC helpers" in help_line or "!query" in help_line

            payload = "integration-irc-payload"
            _send_log_line(log_port, payload)
            broadcast = _wait_for_irc_line(irc_sock, lambda line: payload in line)
            assert payload in broadcast
            assert "PRIVMSG" in broadcast

            irc_sock.sendall(b"PRIVMSG #logs-all :!logstats\r\n")
            stats_line = _wait_for_irc_line(irc_sock, lambda line: "logstats" in line or "channels=" in line)
            assert "channels" in stats_line or "statistics" in stats_line


def _open_log_client(port: int) -> socket.socket:
    sock = socket.create_connection(("127.0.0.1", port), timeout=1.0)
    _read_until(sock, ("LogCrafter",))
    return sock


def _extract_int(line: str, key: str) -> int:
    marker = key + "="
    idx = line.find(marker)
    if idx == -1:
        raise AssertionError(f"Missing key {key!r} in line: {line!r}")
    idx += len(marker)
    end = idx
    while end < len(line) and (line[end].isdigit() or line[end] in "-+"):
        end += 1
    return int(line[idx:end])


def _stats_c(port: int) -> int:
    response = _query_command(port, "STATS")
    assert "STATS:" in response
    active = _extract_int(response, "ClientsActive")
    return active - 1 if active > 0 else 0


def _stats_cpp(port: int) -> tuple[int, int, int]:
    response = _query_command(port, "STATS")
    assert "STATS:" in response
    active_log = _extract_int(response, "ActiveLog")
    active_query = _extract_int(response, "ActiveQuery")
    active_irc = _extract_int(response, "ActiveIRC")
    if active_query > 0:
        active_query -= 1
    return active_log, active_query, active_irc


def integration_connection_determinism() -> None:
    """Sequence: SEQ0097. Confirms client lifecycle counters settle deterministically."""

    # C track lifecycle
    with ServerProcess(binary_path("c")) as c_server:
        c_server.wait_ready([9999, 9998])
        initial_active = _stats_c(9998)
        assert initial_active == 0

        log_clients = [_open_log_client(9999) for _ in range(2)]
        time.sleep(0.2)
        during_active = _stats_c(9998)
        assert during_active == 2

        for sock in log_clients:
            sock.close()
        time.sleep(0.2)
        final_active = _stats_c(9998)
        assert final_active == 0

    # C++ track lifecycle
    log_port = 15220
    query_port = 15221
    with ServerProcess(
        binary_path("cpp"),
        "--log-port",
        str(log_port),
        "--query-port",
        str(query_port),
    ) as cpp_server:
        cpp_server.wait_ready([log_port, query_port])
        initial = _stats_cpp(query_port)
        assert initial == (0, 0, 0)

        cpp_logs = [_open_log_client(log_port) for _ in range(3)]
        time.sleep(0.2)
        active_log, active_query, active_irc = _stats_cpp(query_port)
        assert active_log >= 3
        assert active_query == 0
        assert active_irc == 0

        for sock in cpp_logs:
            sock.close()
        time.sleep(0.2)
        final = _stats_cpp(query_port)
        assert final[0] == 0
        assert final[1] == 0
        assert final[2] == 0


INTEGRATION_CASES = {
    "integration_multi_client_broadcast": integration_multi_client_broadcast,
    "integration_cpp_irc_feature": integration_cpp_irc_feature,
    "integration_connection_determinism": integration_connection_determinism,
}


def main() -> None:
    import argparse

    parser = argparse.ArgumentParser(description="Run LogCrafter integration scenarios")
    parser.add_argument("case", choices=sorted(INTEGRATION_CASES.keys()))
    args = parser.parse_args()
    INTEGRATION_CASES[args.case]()


if __name__ == "__main__":
    main()
