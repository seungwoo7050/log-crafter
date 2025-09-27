"""
Sequence: SEQ0079
Track: Shared
MVP: Step C
Change: Implement Step C smoke scenarios for both C and C++ tracks covering
        bind conflicts, dual listener readiness, shutdown handling, capacity
        limits, IRC boot, and persistence toggles.
Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
       smoke_irc_boot, smoke_persistence_toggle
"""

from __future__ import annotations

import argparse
import shutil
import signal
import socket
import subprocess
import tempfile
import time
from pathlib import Path

from tests.common.runtime import ServerProcess, binary_path, build_dir


def _read_until(sock: socket.socket, substrings: tuple[str, ...], timeout: float = 3.0) -> str:
    sock.settimeout(0.5)
    buffer = ""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(1024)
            if not chunk:
                break
            buffer += chunk.decode(errors="ignore")
            if all(sub in buffer for sub in substrings):
                return buffer
        except socket.timeout:
            continue
    raise AssertionError(f"Did not observe expected strings {substrings!r}; captured={buffer!r}")


def _run_and_capture(command: list[str], timeout: float = 5.0) -> subprocess.CompletedProcess:
    return subprocess.run(command, capture_output=True, text=True, timeout=timeout, check=False)


def smoke_bind_conflict() -> None:
    # C track
    c_binary = binary_path("c")
    conflict_port = 12050
    with ServerProcess(c_binary, "-p", str(conflict_port)) as server:
        server.wait_ready([conflict_port, 9998])
        server.ensure_running()
        result = _run_and_capture([str(c_binary), "-p", str(conflict_port)])
        assert result.returncode != 0, "Expected bind conflict to fail for C server"
        assert "bind" in result.stderr.lower() or "address" in result.stderr.lower()

    # C++ track
    cpp_binary = binary_path("cpp")
    cpp_log = 13050
    cpp_query = 13051
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        server.ensure_running()
        result = _run_and_capture(
            [
                str(cpp_binary),
                "--log-port",
                str(cpp_log),
                "--query-port",
                str(cpp_query),
            ]
        )
        assert result.returncode != 0, "Expected bind conflict to fail for C++ server"
        assert "bind" in result.stderr.lower() or "address" in result.stderr.lower()


def smoke_dual_listener() -> None:
    # C track connectivity
    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        with socket.create_connection(("127.0.0.1", 9999), timeout=1.0) as log_sock:
            banner = log_sock.recv(128).decode(errors="ignore")
            assert "LogCrafter" in banner
        with socket.create_connection(("127.0.0.1", 9998), timeout=1.0) as query_sock:
            banner = query_sock.recv(256).decode(errors="ignore")
            assert "Commands" in banner

    # C++ track connectivity and mismatch rejection
    cpp_binary = binary_path("cpp")
    cpp_log = 14010
    cpp_query = 14011
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        with socket.create_connection(("127.0.0.1", cpp_log), timeout=1.0) as log_sock:
            banner = log_sock.recv(128).decode(errors="ignore")
            assert "LogCrafter" in banner
        with socket.create_connection(("127.0.0.1", cpp_query), timeout=1.0) as query_sock:
            banner = query_sock.recv(256).decode(errors="ignore")
            assert "Commands" in banner or "Welcome" in banner

    mismatch = _run_and_capture(
        [
            str(cpp_binary),
            "--log-port",
            str(cpp_log),
            "--query-port",
            str(cpp_log),
        ]
    )
    assert mismatch.returncode != 0, "Expected identical log/query ports to fail"
    assert "bind" in mismatch.stderr.lower() or "address" in mismatch.stderr.lower()


def smoke_shutdown_signal() -> None:
    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        code = server.terminate(signal.SIGTERM)
        assert code == 0, f"C server shutdown returned {code}"

    cpp_binary = binary_path("cpp")
    cpp_log = 14100
    cpp_query = 14101
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        code = server.terminate(signal.SIGINT)
        assert code == 0, f"C++ server shutdown returned {code}"


def smoke_max_clients() -> None:
    c_binary = binary_path("c")
    with ServerProcess(c_binary, "-c", "1") as server:
        server.wait_ready([9999, 9998])

        primary_log = None
        for _ in range(3):
            candidate = socket.create_connection(("127.0.0.1", 9999), timeout=1.0)
            banner = candidate.recv(128).decode(errors="ignore")
            if "capacity" in banner.lower():
                candidate.close()
                time.sleep(0.1)
                continue
            primary_log = candidate
            break
        if primary_log is None:
            raise AssertionError("Unable to secure primary log connection for capacity test")

        try:
            time.sleep(0.1)
            with socket.create_connection(("127.0.0.1", 9999), timeout=1.0) as secondary:
                transcript = []
                secondary.settimeout(0.5)
                deadline = time.monotonic() + 1.0
                while time.monotonic() < deadline:
                    try:
                        chunk = secondary.recv(256)
                    except ConnectionResetError:
                        break
                    except socket.timeout:
                        continue
                    if not chunk:
                        break
                    text = chunk.decode(errors="ignore")
                    transcript.append(text)
                    if "capacity" in text.lower():
                        break
                message = "".join(transcript)
                assert "capacity" in message.lower(), f"unexpected response: {message!r}"
        finally:
            primary_log.close()


def smoke_irc_boot() -> None:
    cpp_binary = binary_path("cpp")
    cpp_log = 14200
    cpp_query = 14201
    cpp_irc = 14202
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
        "--enable-irc",
        "--irc-port",
        str(cpp_irc),
        "--irc-server-name",
        "smoke.local",
        "--irc-auto-join",
        "#logs-test",
    ) as server:
        server.wait_ready([cpp_log, cpp_query, cpp_irc])
        with socket.create_connection(("127.0.0.1", cpp_irc), timeout=2.0) as irc_sock:
            irc_sock.sendall(b"NICK tester\r\nUSER tester 0 * :Smoke Test\r\n")
            transcript = _read_until(irc_sock, ("001 tester", "Try !help"))
            assert "#logs-test" in transcript or "JOIN" in transcript


def smoke_persistence_toggle() -> None:
    build = build_dir()
    default_logs = build / "logs"
    if default_logs.exists():
        if default_logs.is_dir():
            shutil.rmtree(default_logs)
        else:
            default_logs.unlink()

    # C track disabled by default
    c_binary = binary_path("c")
    with ServerProcess(c_binary) as server:
        server.wait_ready([9999, 9998])
        server.terminate()
    assert not default_logs.exists(), "C server should not create logs directory when persistence disabled"

    # C track enabled
    tmp_root = Path(tempfile.mkdtemp(prefix="lc-smoke-c-", dir=str(build)))
    enabled_dir = tmp_root / "persist"
    with ServerProcess(c_binary, "-P", "-d", str(enabled_dir)) as server:
        server.wait_ready([9999, 9998])
        assert enabled_dir.exists(), "Persistence directory should be created"
        server.terminate()
    shutil.rmtree(tmp_root)

    # C++ track disabled by default
    if default_logs.exists():
        if default_logs.is_dir():
            shutil.rmtree(default_logs)
        else:
            default_logs.unlink()

    cpp_binary = binary_path("cpp")
    cpp_log = 14300
    cpp_query = 14301
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
        "--disable-persistence",
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        server.terminate()
    assert not default_logs.exists(), "C++ server should not create logs directory when persistence disabled"

    # C++ track enabled
    tmp_root = Path(tempfile.mkdtemp(prefix="lc-smoke-cpp-", dir=str(build)))
    enabled_dir = tmp_root / "persist"
    with ServerProcess(
        cpp_binary,
        "--log-port",
        str(cpp_log),
        "--query-port",
        str(cpp_query),
        "--enable-persistence",
        "--persistence-dir",
        str(enabled_dir),
    ) as server:
        server.wait_ready([cpp_log, cpp_query])
        assert enabled_dir.exists(), "C++ persistence directory should be created"
        server.terminate()
    shutil.rmtree(tmp_root)


SMOKE_CASES = {
    "smoke_bind_conflict": smoke_bind_conflict,
    "smoke_dual_listener": smoke_dual_listener,
    "smoke_shutdown_signal": smoke_shutdown_signal,
    "smoke_max_clients": smoke_max_clients,
    "smoke_irc_boot": smoke_irc_boot,
    "smoke_persistence_toggle": smoke_persistence_toggle,
}


def main() -> None:
    parser = argparse.ArgumentParser(description="Run LogCrafter smoke scenarios")
    parser.add_argument("case", choices=sorted(SMOKE_CASES.keys()))
    args = parser.parse_args()
    SMOKE_CASES[args.case]()


if __name__ == "__main__":
    main()
