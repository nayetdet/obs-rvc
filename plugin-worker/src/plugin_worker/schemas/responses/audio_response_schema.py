from __future__ import annotations

import ctypes

from ...settings import Settings
from .base_response_schema import BaseResponseSchema


class AudioResponseSchema(BaseResponseSchema):
    _fields_ = [
        ("status", ctypes.c_uint8),
        ("audio_size", ctypes.c_uint32),
        ("sample_rate", ctypes.c_uint32),
        ("error_size", ctypes.c_uint32),
        ("error", ctypes.c_char * Settings.max_error_bytes),
        ("audio", ctypes.c_uint8 * Settings.max_output_bytes),
    ]
