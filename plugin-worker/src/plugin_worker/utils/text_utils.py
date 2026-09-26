from __future__ import annotations


def decode_c_string(value: object) -> str:
    return bytes(value).split(b"\0", 1)[0].decode("utf-8", errors="replace")
