from __future__ import annotations

import ctypes

from ...settings import Settings


class AudioRequestSchema(ctypes.Structure):
    _fields_ = [
        ("action", ctypes.c_uint8),
        ("audio_size", ctypes.c_uint32),
        ("model", ctypes.c_char * Settings.max_model_bytes),
        ("input_format", ctypes.c_char * 8),
        ("speaker", ctypes.c_int32),
        ("f0_up_key", ctypes.c_int32),
        ("f0_method", ctypes.c_char * 8),
        ("index_file", ctypes.c_char * Settings.max_index_path_bytes),
        ("index_rate", ctypes.c_float),
        ("filter_radius", ctypes.c_int32),
        ("resample_sr", ctypes.c_int32),
        ("rms_mix_rate", ctypes.c_float),
        ("protect", ctypes.c_float),
        ("audio", ctypes.c_uint8 * Settings.max_audio_bytes),
    ]
