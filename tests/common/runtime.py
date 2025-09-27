"""
Sequence: SEQ0079
Track: Shared
MVP: Step C
Change: Introduce reusable helpers for launching servers, waiting on ports, and
        terminating processes to support Step C smoke automation.
Tests: smoke_bind_conflict, smoke_dual_listener, smoke_shutdown_signal, smoke_max_clients,
       smoke_irc_boot, smoke_persistence_toggle
"""

from __future__ import annotations

import os
import signal
import socket
import subprocess
import time
from pathlib import Path
from typing import Iterable, Optional


def build_dir() -> Path:
    build_root = os.environ.get("LOGCRAFTER_BUILD_DIR")
    if not build_root:
        raise RuntimeError("LOGCRAFTER_BUILD_DIR is not set")
    path = Path(build_root).resolve()
    if not path.exists():
        raise RuntimeError(f"Build directory does not exist: {path}")
    return path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def wait_for_port(port: int, process: Optional[subprocess.Popen] = None, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    last_error: Optional[Exception] = None
    while time.monotonic() < deadline:
        if process is not None and process.poll() is not None:
            raise AssertionError(
                f"Process exited early with code {process.returncode}."
            )
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.5):
                return
        except OSError as exc:
            last_error = exc
            time.sleep(0.05)
    raise AssertionError(f"Timed out waiting for port {port} to accept connections: {last_error}")


class ServerProcess:
    """Context manager for launching and stopping LogCrafter binaries."""

    def __init__(self, binary: Path, *args: str) -> None:
        self.binary = Path(binary)
        if not self.binary.exists():
            raise FileNotFoundError(f"Binary not found: {self.binary}")
        command = [str(self.binary), *args]
        self._proc = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        self.stdout: str = ""
        self.stderr: str = ""

    @property
    def process(self) -> subprocess.Popen:
        return self._proc

    def wait_ready(self, ports: Iterable[int], timeout: float = 5.0) -> None:
        for port in ports:
            wait_for_port(port, self._proc, timeout)

    def terminate(self, sig: signal.Signals = signal.SIGTERM, timeout: float = 5.0) -> int:
        if self._proc.poll() is None:
            self._proc.send_signal(sig)
        try:
            out, err = self._proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            self._proc.kill()
            out, err = self._proc.communicate(timeout=timeout)
        self.stdout += out
        self.stderr += err
        return self._proc.returncode or 0

    def ensure_running(self) -> None:
        if self._proc.poll() is not None:
            raise AssertionError(f"Process terminated unexpectedly with code {self._proc.returncode}.")

    def __enter__(self) -> "ServerProcess":
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._proc.poll() is None:
            try:
                self.terminate()
            except Exception:
                pass


def binary_path(track: str) -> Path:
    build = build_dir()
    if track == "c":
        return build / "work" / "c" / "logcrafter_c_mvp5"
    if track == "cpp":
        return build / "work" / "cpp" / "logcrafter_cpp_mvp6"
    raise ValueError(f"Unknown track: {track}")

