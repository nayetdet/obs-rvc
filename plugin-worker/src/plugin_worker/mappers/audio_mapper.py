from __future__ import annotations

from ..schemas.responses.audio_response_schema import AudioResponseSchema
from ..enums.ipc_response_status_enum import IPCResponseStatusEnum
from ..settings import Settings


class AudioMapper:
    @staticmethod
    def from_error_message(message: str) -> AudioResponseSchema:
        response: AudioResponseSchema = AudioResponseSchema()
        response.status = IPCResponseStatusEnum.ERROR
        encoded: bytes = message.encode("utf-8")[: Settings.max_error_bytes - 1]
        response.error = encoded
        response.error_size = len(encoded)
        return response

    @staticmethod
    def from_audio(audio: bytes, sample_rate: int) -> AudioResponseSchema:
        response: AudioResponseSchema = AudioResponseSchema()
        response.status = IPCResponseStatusEnum.OK
        response.audio_size = len(audio)
        response.sample_rate = sample_rate
        response.audio[: len(audio)] = audio
        return response
