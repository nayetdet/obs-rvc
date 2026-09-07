from __future__ import annotations


class TextUtils:
    @staticmethod
    def decode(value: object) -> str:
        return bytes(value).split(b"\0", 1)[0].decode("utf-8", errors="replace")
