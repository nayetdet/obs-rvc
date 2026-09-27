from __future__ import annotations

import logging

import numpy as np
from pydantic import ValidationError

from ...core.rvc_inference import RVCInference
from ...exceptions.rvc_inference_error import RVCInferenceError
from ...mappers.response_mapper import audio_error, audio_success
from ...schemas.requests.audio_request_schema import AudioRequestSchema
from ...schemas.internal.rvc_inference_options_schema import RVCInferenceOptionsSchema
from ...schemas.responses.audio_response_schema import AudioResponseSchema
from ...settings import Settings, settings
from ...utils.text_utils import decode_c_string
from .base_handler import BaseHandler

logger = logging.getLogger(__name__)


class RVCHandler(BaseHandler[AudioRequestSchema, AudioResponseSchema]):
    def __init__(self, rvc: RVCInference) -> None:
        super().__init__(
            settings.service_name,
            AudioRequestSchema,
            AudioResponseSchema,
        )

        self.rvc = rvc
        self.last_unexpected_error: str | None = None

    def handle(self, request: AudioRequestSchema) -> AudioResponseSchema:
        if request.audio_size == 0 or request.audio_size > Settings.max_audio_bytes:
            return audio_error("Audio size is invalid.")

        if request.sample_rate == 0 or request.audio_size % 4 != 0:
            return audio_error("Audio format is invalid.")

        try:
            options: RVCInferenceOptionsSchema = RVCInferenceOptionsSchema(
                speaker=request.speaker,
                f0_up_key=request.f0_up_key,
                f0_method=decode_c_string(request.f0_method),
                filter_radius=request.filter_radius,
                resample_sr=request.resample_sr,
                rms_mix_rate=request.rms_mix_rate,
                protect=request.protect,
                index_rate=request.index_rate,
            )

            output: np.ndarray
            sample_rate: int
            output, sample_rate = self.rvc.convert(
                np.ctypeslib.as_array(request.audio)[: request.audio_size].view("<f4"),
                request.sample_rate,
                decode_c_string(request.model) or None,
                options,
            )

            if output.nbytes > Settings.max_output_bytes:
                return audio_error("Converted audio exceeds the IPC buffer.")
            self.last_unexpected_error = None
            return audio_success(output, sample_rate)
        except ValidationError:
            return audio_error("Inference options are invalid.")
        except RVCInferenceError as exc:
            return audio_error(str(exc))
        except Exception as exc:
            message = f"{type(exc).__name__}: {exc}"
            if message != self.last_unexpected_error:
                logger.exception("Unexpected error while converting audio")
                self.last_unexpected_error = message
            return audio_error(f"RVC conversion failed: {exc}")
