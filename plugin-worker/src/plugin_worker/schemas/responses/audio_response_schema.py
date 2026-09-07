from __future__ import annotations

import ctypes

from ...settings import Settings


class AudioResponseSchema(ctypes.Structure):
    _fields_ = [
        ("status", ctypes.c_uint8),
        ("audio_size", ctypes.c_uint32),
        ("sample_rate", ctypes.c_uint32),
        ("error_size", ctypes.c_uint32),
        ("error", ctypes.c_char * Settings.max_error_bytes),
        ("audio", ctypes.c_uint8 * Settings.max_output_bytes),
    ]

    @classmethod
    def from_error(cls, message: str) -> AudioResponseSchema:
        response = cls()
        response.status = Settings.status_error
        encoded: bytes = message.encode("utf-8")[: Settings.max_error_bytes - 1]
        response.error = encoded
        response.error_size = len(encoded)
        return response
