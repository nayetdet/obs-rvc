from __future__ import annotations

from ..enums.ipc_response_status_enum import IPCResponseStatusEnum
from ..schemas.responses.audio_response_schema import AudioResponseSchema
from ..schemas.responses.settings_response_schema import SettingsResponseSchema
from ..settings import Settings

import numpy as np


def audio_error(message: str) -> AudioResponseSchema:
    response = AudioResponseSchema()
    response.status = IPCResponseStatusEnum.ERROR
    encoded = message.encode("utf-8")[: Settings.max_error_bytes - 1]
    response.error = encoded
    response.error_size = len(encoded)
    return response


def audio_success(audio: np.ndarray, sample_rate: int) -> AudioResponseSchema:
    response = AudioResponseSchema()
    response.status = IPCResponseStatusEnum.OK
    pcm = np.ascontiguousarray(audio, dtype="<f4").tobytes()
    response.audio_size = len(pcm)
    response.sample_rate = sample_rate
    response.audio[: len(pcm)] = pcm
    return response


def settings_error(message: str) -> SettingsResponseSchema:
    response = SettingsResponseSchema()
    response.status = IPCResponseStatusEnum.ERROR
    encoded = message.encode("utf-8")[: Settings.max_error_bytes - 1]
    response.error = encoded
    response.error_size = len(encoded)
    return response


def settings_success() -> SettingsResponseSchema:
    response = SettingsResponseSchema()
    response.status = IPCResponseStatusEnum.OK
    return response
