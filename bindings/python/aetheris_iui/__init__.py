"""
aetheris_iui - Python binding for Aetheris-IUI via stdio IPC.

Uses the aetheris_stdio_server binary as the backend; no native compilation
required.  For performance-critical workloads, prefer the pybind11 native
binding (see bindings/python/pybind11_stub.cpp) which wraps the C++ API
directly.
"""

from .client import AetherisClient, AetherisError

__all__ = ["AetherisClient", "AetherisError"]
