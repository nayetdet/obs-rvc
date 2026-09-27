from __future__ import annotations

import logging
import os
import threading
from collections import OrderedDict
from pathlib import Path
from typing import Any

import numpy as np

from ...exceptions.rvc_inference_exceptions import (
    RVCAudioException,
    RVCConfigurationException,
    RVCInferenceException,
    RVCIndexException,
    RVCInferenceModelNotFoundException,
)
from ...runtime import Runtime, runtime
from ...utils.compatibility_utils import configure_torch
from ...utils.inference_utils import bake_weight_norm
from .rvc_inference_streaming import infer_window
from .rvc_index import load_rvc_index
from ..streaming.streaming_highpass_filter import StreamingHighpassFilter

logger = logging.getLogger(__name__)


class RVCInference:
    def __init__(self) -> None:
        self.models: dict[str, Any] = {}
        self.model_errors: dict[str, str] = {}
        self.model_locks: dict[str, threading.Lock] = {}
        self.realtime_pitch_warning_models: set[str] = set()
        self.hubert_models: dict[tuple[str, str, bool], Runtime.Hubert] = {}
        self.indices: dict[tuple[str, str], tuple[Any, np.ndarray]] = {}
        self.lock = threading.Lock()
        self.vc_class: Any = None

    def convert(
        self,
        audio: Any,
        sample_rate: int,
        stream_id: int,
        stream_generation: int,
        model: str | None,
        index_path: str | None,
        options: Runtime.Options,
    ) -> tuple[np.ndarray, int]:
        if np.asarray(audio).size == 0 or sample_rate <= 0:
            raise RVCAudioException()

        if runtime.hubert_path is None or runtime.rmvpe_path is None:
            raise RVCConfigurationException("HuBERT and RMVPE paths must be configured.")

        if not model and not runtime.model:
            raise RVCInferenceModelNotFoundException()

        model_path = Path(model or runtime.model or "").expanduser().resolve()
        if model_path.suffix.lower() != ".pth" or not model_path.is_file():
            raise RVCInferenceModelNotFoundException()

        index_file: Path | None = None
        if index_path:
            index_file = Path(index_path).expanduser().resolve()
            if index_file.suffix.lower() != ".index" or not index_file.is_file():
                raise RVCIndexException("RVC index file was not found.")

        with self.lock:
            if self.vc_class is None:
                hubert_path = runtime.hubert_path.expanduser().resolve()
                rmvpe_path = runtime.rmvpe_path.expanduser().resolve()
                if not hubert_path.is_file():
                    raise RVCConfigurationException("HuBERT model file was not found.")

                if not rmvpe_path.is_file() or rmvpe_path.name != "rmvpe.pt":
                    raise RVCConfigurationException("RMVPE model file is missing or invalid.")

                os.environ.update(
                    hubert_path=str(hubert_path),
                    rmvpe_root=str(rmvpe_path.parent),
                    weight_root=str(model_path.parent),
                    index_root=str(model_path.parent.parent / "index"),
                )

                effective_threads = configure_torch(runtime.inference_threads, runtime.obs_reserved_threads)
                if effective_threads != runtime.inference_threads:
                    logger.info(
                        "Limiting %d requested RVC threads to %d after reserving %d threads for OBS.",
                        runtime.inference_threads,
                        effective_threads,
                        runtime.obs_reserved_threads,
                    )

                from rvc.modules.vc import modules as vc_modules
                self.vc_class = vc_modules.VC

            key: str = str(model_path)
            if key in self.model_errors:
                raise RVCInferenceException(self.model_errors[key])

            if key not in self.models:
                logger.info("Loading RVC model: %s", model_path)
                try:
                    vc: Any = self.vc_class()
                    vc.get_vc(key)
                    vc.speakers = {}
                    from rvc.modules.vc.pipeline import ah, bh

                    vc.input_filter_coefficients = (bh, ah)
                    vc.streams = OrderedDict()
                    removed_weight_norms = bake_weight_norm(vc.net_g)
                    if removed_weight_norms:
                        logger.info(
                            "Optimized RVC generator for inference by baking %d weight-normalized layers.",
                            removed_weight_norms,
                        )

                    hubert_path = str(runtime.hubert_path.expanduser().resolve())
                    hubert_key = (hubert_path, str(vc.config.device), bool(vc.config.is_half))
                    hubert_resource = self.hubert_models.get(hubert_key)
                    if hubert_resource is None:
                        from rvc.modules.vc.utils import load_hubert
                        hubert_resource = Runtime.Hubert(model=load_hubert(vc.config, hubert_path))
                        self.hubert_models[hubert_key] = hubert_resource

                    vc.hubert_model = hubert_resource.model
                    vc.hubert_lock = hubert_resource.lock
                    logger.info("RVC inference device: %s", vc.config.device)
                except Exception as exc:
                    logger.exception("Unable to load RVC model: %s", model_path)
                    message = f"Unable to load RVC model '{model_path.name}'."
                    self.model_errors[key] = message
                    raise RVCInferenceException(message) from exc
                self.models[key] = vc

            vc: Any = self.models[key]
            index: tuple[Any, np.ndarray] | None = None
            if index_file is not None and options.index_rate > 0.0:
                index_key = (key, str(index_file))
                index = self.indices.get(index_key)
                if index is None:
                    index = load_rvc_index(str(index_file), vc.version)
                    self.indices[index_key] = index
                    logger.info("Loaded RVC index: %s (%d vectors)", index_file, index[0].ntotal)
            model_lock: threading.Lock = self.model_locks.setdefault(key, threading.Lock())

        with model_lock:
            vc: Any = self.models[key]
            stream = vc.streams.get(stream_id)
            if stream is None or stream.generation != stream_generation:
                stream = Runtime.Stream(generation=stream_generation)
                stream.input_filter = StreamingHighpassFilter(*vc.input_filter_coefficients)
                vc.streams[stream_id] = stream

            vc.streams.move_to_end(stream_id)
            while len(vc.streams) > 16:
                vc.streams.popitem(last=False)

            if str(vc.config.device) == "cpu" and options.f0_method != "pm":
                if key not in self.realtime_pitch_warning_models:
                    logger.warning("Using PM pitch tracking for realtime CPU conversion.")
                    self.realtime_pitch_warning_models.add(key)
                options = options.model_copy(update={"f0_method": "pm"})

            output, target_sr, timings = infer_window(vc, stream, audio, sample_rate, options, index)
            logger.debug(
                "RVC timings ms: input_resample=%.1f filter=%.1f f0=%.1f hubert=%.1f generator=%.1f rms=%.1f output_resample=%.1f",
                timings["input_resample"] * 1000,
                timings["filter"] * 1000,
                timings["f0"] * 1000,
                timings["hubert"] * 1000,
                timings["generator"] * 1000,
                timings["rms"] * 1000,
                timings["output_resample"] * 1000,
            )

            return output, target_sr

    def warmup(self) -> None:
        options = Runtime.Options()
        silence = np.zeros(16_800, dtype=np.int16)
        self.convert(silence, 48_000, 0, 0, None, None, options)
        with self.lock:
            for vc in self.models.values():
                vc.streams.pop(0, None)

    def reset(self) -> None:
        with self.lock:
            self.models.clear()
            self.model_errors.clear()
            self.model_locks.clear()
            self.realtime_pitch_warning_models.clear()
            self.hubert_models.clear()
            self.indices.clear()
            self.vc_class = None
