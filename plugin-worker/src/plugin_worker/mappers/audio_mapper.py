from __future__ import annotations

import numpy as np

from ..enums.ipc_response_status_enum import IPCResponseStatusEnum
from ..schemas.responses.audio_response_schema import AudioResponseSchema
from ..settings import Settings


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
    normalized = np.clip(np.nan_to_num(audio, copy=False), -1.0, 1.0)
    pcm = np.minimum(np.floor(normalized * 32768.0), 32767.0).astype("<i2").tobytes()
    response.audio_size = len(pcm)
    response.sample_rate = sample_rate
    response.audio[: len(pcm)] = pcm
    return response
