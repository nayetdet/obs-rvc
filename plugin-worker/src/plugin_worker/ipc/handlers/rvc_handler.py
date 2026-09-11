from __future__ import annotations

from pydantic import ValidationError

from ...core.rvc_inference import RVCInference
from ...exceptions.rvc_inference_error import RVCInferenceError
from ...mappers.audio_mapper import AudioMapper
from .base_handler import BaseHandler
from ...schemas.requests.audio_request_schema import AudioRequestSchema
from ...schemas.internal.rvc_inference_options_schema import RVCInferenceOptionsSchema
from ...schemas.responses.audio_response_schema import AudioResponseSchema
from ...settings import settings
from ...utils.text_utils import TextUtils
from ..publishers.audio_publisher import AudioPublisher
from ..subscribers.audio_subscriber import AudioSubscriber


class RVCHandler(BaseHandler[AudioRequestSchema, AudioResponseSchema]):
    def __init__(self, rvc: RVCInference) -> None:
        super().__init__(
            settings.service_name,
            AudioRequestSchema,
            AudioResponseSchema,
            AudioPublisher(),
            AudioSubscriber(),
        )

        self.rvc = rvc

    def handle(self, request: AudioRequestSchema) -> AudioResponseSchema:
        if request.audio_size == 0 or request.audio_size > settings.max_audio_bytes:
            return AudioMapper.from_error_message("Audio size is invalid.")

        try:
            options: RVCInferenceOptionsSchema = RVCInferenceOptionsSchema(
                speaker=request.speaker,
                f0_up_key=request.f0_up_key,
                f0_method=TextUtils.decode(request.f0_method),
                index_file=TextUtils.decode(request.index_file) or None,
                index_rate=request.index_rate,
                filter_radius=request.filter_radius,
                resample_sr=request.resample_sr,
                rms_mix_rate=request.rms_mix_rate,
                protect=request.protect,
            )

            output: bytes
            sample_rate: int
            output, sample_rate = self.rvc.convert(
                bytes(request.audio[: request.audio_size]),
                TextUtils.decode(request.model) or None,
                TextUtils.decode(request.input_format) or "wav",
                options,
            )

            if len(output) > settings.max_output_bytes:
                return AudioMapper.from_error_message("Converted audio exceeds the IPC buffer.")
            return AudioMapper.from_audio(output, sample_rate)
        except ValidationError:
            return AudioMapper.from_error_message("Inference options are invalid.")
        except RVCInferenceError as exc:
            return AudioMapper.from_error_message(str(exc))
