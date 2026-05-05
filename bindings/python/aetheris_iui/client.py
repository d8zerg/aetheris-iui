"""Stdio IPC client for the Aetheris-IUI server process."""

from __future__ import annotations

import json
import subprocess
import threading
from pathlib import Path
from typing import Any


class AetherisError(RuntimeError):
    pass


class AetherisClient:
    """
    Connects to an aetheris_stdio_server subprocess.

    Usage::

        with AetherisClient("/path/to/aetheris_stdio_server") as client:
            print(client.version())
            print(client.session_snapshot(session_dict))
    """

    def __init__(self, server_path: str | Path) -> None:
        self._proc = subprocess.Popen(
            [str(server_path)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self._lock = threading.Lock()

    # ── Public API ──────────────────────────────────────────────────────────

    def ping(self) -> dict[str, Any]:
        """Returns {"type": "pong", "version": "..."}."""
        return self._request({"type": "ping"})

    def version(self) -> dict[str, Any]:
        """Returns {"type": "version", "version": "...", "abi": <int>}."""
        return self._request({"type": "version"})

    def session_snapshot(self, session: dict[str, Any]) -> dict[str, Any]:
        """
        Projects a session dict through the interface layer.

        Returns {"type": "snapshot", "snapshot": {...}} or raises AetherisError.
        """
        resp = self._request({"type": "snapshot", "session": session})
        if resp.get("type") == "error":
            raise AetherisError(resp.get("message", "unknown server error"))
        return resp

    def quit(self) -> None:
        """Requests graceful shutdown; the process exits after sending 'bye'."""
        try:
            self._request({"type": "quit"})
        except (BrokenPipeError, OSError):
            pass
        finally:
            if self._proc.stdin:
                self._proc.stdin.close()

    # ── Context manager ──────────────────────────────────────────────────────

    def __enter__(self) -> "AetherisClient":
        return self

    def __exit__(self, *_: object) -> None:
        try:
            self.quit()
        except Exception:
            pass
        self._proc.wait(timeout=5)

    # ── Internal ─────────────────────────────────────────────────────────────

    def _request(self, msg: dict[str, Any]) -> dict[str, Any]:
        with self._lock:
            assert self._proc.stdin and self._proc.stdout
            self._proc.stdin.write(json.dumps(msg) + "\n")
            self._proc.stdin.flush()
            line = self._proc.stdout.readline()
            if not line:
                raise AetherisError("server closed the connection unexpectedly")
            return json.loads(line)
