"""
Smoke tests for the Python aetheris_iui binding.

Run standalone:
    python3 test_smoke.py --server /path/to/aetheris_stdio_server

Run via ctest (server path is passed automatically).
"""

import argparse
import sys
from pathlib import Path

# Allow running without installing the package
sys.path.insert(0, str(Path(__file__).parent.parent))
from aetheris_iui import AetherisClient

_SAMPLE_SESSION = {
    "id": "sess-001",
    "action_id": "camera.start_recording",
    "operator_id": "op-alice",
    "tenant_id": "tenant-acme",
    "state": "fill",
    "confirmation_mode": "single",
    "slots": [
        {"name": "camera_id", "required": True, "value_json": None},
        {"name": "resolution", "required": False, "value_json": '"1080p"'},
    ],
    "clarification_question": None,
    "preview_result_json": None,
    "archive_reason": None,
    "created_at_us": 1_000_000_000,
    "updated_at_us": 1_000_000_000,
}


def test_ping(server_path: str) -> None:
    with AetherisClient(server_path) as client:
        resp = client.ping()
    assert resp["type"] == "pong", f"unexpected type: {resp}"
    assert "version" in resp, "version missing from pong"
    print(f"  ok  ping -> version={resp['version']}")


def test_version(server_path: str) -> None:
    with AetherisClient(server_path) as client:
        resp = client.version()
    assert resp["type"] == "version", f"unexpected type: {resp}"
    assert isinstance(resp["abi"], int), "abi must be an integer"
    assert resp["abi"] > 0, "abi must be positive"
    print(f"  ok  version={resp['version']} abi={resp['abi']}")


def test_session_snapshot(server_path: str) -> None:
    with AetherisClient(server_path) as client:
        resp = client.session_snapshot(_SAMPLE_SESSION)
    assert resp["type"] == "snapshot", f"unexpected type: {resp}"
    snap = resp["snapshot"]
    assert snap["id"] == "sess-001"
    assert snap["action_id"] == "camera.start_recording"
    assert snap["state"] == "fill"
    assert snap["confirmation_mode"] == "single"
    assert isinstance(snap["slots"], list)
    assert len(snap["slots"]) == 2
    assert snap["created_at_us"] == 1_000_000_000
    print(f"  ok  snapshot id={snap['id']} slots={len(snap['slots'])}")


def test_multiple_requests_same_client(server_path: str) -> None:
    with AetherisClient(server_path) as client:
        pong = client.ping()
        ver = client.version()
        snap = client.session_snapshot(_SAMPLE_SESSION)
    assert pong["type"] == "pong"
    assert ver["type"] == "version"
    assert snap["type"] == "snapshot"
    print("  ok  multiple requests per client")


def test_unknown_type_returns_error(server_path: str) -> None:
    with AetherisClient(server_path) as client:
        resp = client._request({"type": "nonexistent_command"})
    assert resp["type"] == "error", f"expected error, got: {resp}"
    print("  ok  unknown type returns error")


def main() -> int:
    parser = argparse.ArgumentParser(description="Aetheris-IUI Python binding smoke tests")
    parser.add_argument("--server", required=True, help="Path to aetheris_stdio_server binary")
    args = parser.parse_args()

    tests = [
        test_ping,
        test_version,
        test_session_snapshot,
        test_multiple_requests_same_client,
        test_unknown_type_returns_error,
    ]

    failed = 0
    for test_fn in tests:
        name = test_fn.__name__
        try:
            test_fn(args.server)
        except Exception as exc:
            print(f" FAIL {name}: {exc}")
            failed += 1

    total = len(tests)
    print(f"\n{total - failed}/{total} smoke tests passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
