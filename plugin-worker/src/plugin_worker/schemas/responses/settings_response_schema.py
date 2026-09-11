from __future__ import annotations

import ctypes

from ...settings import settings
from .base_response_schema import BaseResponseSchema


class SettingsResponseSchema(BaseResponseSchema):
    _fields_ = [
        ("status", ctypes.c_uint8),
        ("error_size", ctypes.c_uint32),
        ("error", ctypes.c_char * settings.max_error_bytes),
    ]
