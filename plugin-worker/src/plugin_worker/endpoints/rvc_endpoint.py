from __future__ import annotations

import logging
from concurrent.futures import Future, ThreadPoolExecutor
from threading import Lock
from typing import Any

import numpy as np
from pydantic import ValidationError

from ..core.rvc.rvc_inference import RVCInference
from ..exceptions.rvc_inference_exceptions import RVCInferenceException
from ..mappers.audio_mapper import audio_error, audio_success
from ..schemas.requests.audio_request_schema import AudioRequestSchema
from ..runtime import Runtime
from ..schemas.responses.audio_response_schema import AudioResponseSchema
from ..settings import Settings, settings
from ..utils.text_utils import decode_c_string
from .base_endpoint import BaseEndpoint

logger = logging.getLogger(__name__)


class RVCEndpoint(BaseEndpoint[AudioRequestSchema, AudioResponseSchema]):
    def __init__(self, rvc: RVCInference) -> None:
        super().__init__(
            settings.service_name,
            AudioRequestSchema,
            AudioResponseSchema,
        )

        self.rvc = rvc
        self.executor = ThreadPoolExecutor(max_workers=settings.parallel_workers, thread_name_prefix="rvc")
        self.pending: set[Future[None]] = set()
        self.pending_lock = Lock()

    def process(self, server: Any) -> bool:
        with self.pending_lock:
            if len(self.pending) >= settings.parallel_workers:
                return False
        request = server.receive()
        if request is None:
            return False
        future = self.executor.submit(self.reply, request)
        with self.pending_lock:
            self.pending.add(future)
        future.add_done_callback(self.complete)
        return True

    def complete(self, future: Future[None]) -> None:
        with self.pending_lock:
            self.pending.discard(future)

    def reply(self, request: Any) -> None:
        try:
            request.send_copy(self.handle(request.payload().contents))
        finally:
            request.delete()

    def close(self) -> None:
        self.executor.shutdown(wait=True)

    def handle(self, request: AudioRequestSchema) -> AudioResponseSchema:
        if request.audio_size == 0 or request.audio_size > Settings.max_audio_bytes:
            return audio_error("Audio size is invalid.")

        if request.sample_rate == 0 or request.audio_size % 2 != 0:
            return audio_error("Audio format is invalid.")

        try:
            options: Runtime.Options = Runtime.Options(
                speaker=request.speaker,
                f0_up_key=request.f0_up_key,
                f0_method=decode_c_string(request.f0_method),
                filter_radius=request.filter_radius,
                resample_sr=request.resample_sr,
                rms_mix_rate=request.rms_mix_rate,
                protect=request.protect,
            )

            output: np.ndarray
            sample_rate: int
            output, sample_rate = self.rvc.convert(
                np.ctypeslib.as_array(request.audio)[: request.audio_size].view("<i2"),
                request.sample_rate,
                request.stream_id,
                request.stream_generation,
                decode_c_string(request.model) or None,
                options,
            )

            if output.nbytes > Settings.max_output_bytes:
                return audio_error("Converted audio exceeds the IPC buffer.")
            return audio_success(output, sample_rate)
        except ValidationError:
            return audio_error("Inference options are invalid.")
        except RVCInferenceException as exc:
            return audio_error(str(exc))
        except Exception as exc:
            logger.exception("Unexpected error while converting audio")
            return audio_error(f"RVC conversion failed: {exc}")
