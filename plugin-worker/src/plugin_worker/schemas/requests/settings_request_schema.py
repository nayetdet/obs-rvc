from __future__ import annotations

import ctypes

from ...settings import settings
from .base_request_schema import BaseRequestSchema


class SettingsRequestSchema(BaseRequestSchema):
    _fields_ = [
        ("model", ctypes.c_char * settings.max_model_bytes),
        ("model_dir", ctypes.c_char * settings.max_index_path_bytes),
        ("hubert_path", ctypes.c_char * settings.max_index_path_bytes),
        ("rmvpe_root", ctypes.c_char * settings.max_index_path_bytes),
    ]
