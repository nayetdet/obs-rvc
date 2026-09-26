from __future__ import annotations

import logging
import os
import threading
from pathlib import Path
from typing import Any

from ..exceptions.rvc_inference_error import RVCInferenceError
from ..exceptions.rvc_inference_model_not_found import RVCInferenceModelNotFoundError
from ..schemas.internal.rvc_inference_options_schema import RVCInferenceOptionsSchema
from ..runtime import runtime
from ..utils.audio_utils import encode_wav
from ..utils.compatibility_utils import configure_torch
from ..utils.inference_utils import bake_weight_norm
from .rvc_inference_streaming import infer_window

logger = logging.getLogger(__name__)

class RVCInference:
    def __init__(self) -> None:
        self.models: dict[str, Any] = {}
        self.model_errors: dict[str, str] = {}
        self.model_locks: dict[str, threading.Lock] = {}
        self.realtime_pitch_warning_models: set[str] = set()
        self.lock = threading.Lock()
        self.vc_class: Any = None

    def convert(
        self,
        audio: bytes,
        model: str | None,
        options: RVCInferenceOptionsSchema,
    ) -> tuple[bytes, int]:
        if not audio:
            raise RVCInferenceError()

        if runtime.hubert_path is None or runtime.rmvpe_path is None:
            raise RVCInferenceError()

        if not model and not runtime.model:
            raise RVCInferenceModelNotFoundError()

        model_path = Path(model or runtime.model or "").expanduser().resolve()
        if model_path.suffix.lower() != ".pth" or not model_path.is_file():
            raise RVCInferenceModelNotFoundError()

        with self.lock:
            if self.vc_class is None:
                hubert_path = runtime.hubert_path.expanduser().resolve()
                rmvpe_path = runtime.rmvpe_path.expanduser().resolve()
                if not hubert_path.is_file():
                    raise RVCInferenceError()

                if not rmvpe_path.is_file() or rmvpe_path.name != "rmvpe.pt":
                    raise RVCInferenceError()

                os.environ.update(
                    hubert_path=str(hubert_path),
                    rmvpe_root=str(rmvpe_path.parent),
                    weight_root=str(model_path.parent),
                    index_root=str(model_path.parent),
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
                raise RVCInferenceError(self.model_errors[key])

            if key not in self.models:
                logger.info("Loading RVC model: %s", model_path)
                try:
                    vc: Any = self.vc_class()
                    vc.get_vc(key)
                    removed_weight_norms = bake_weight_norm(vc.net_g)
                    if removed_weight_norms:
                        logger.info(
                            "Optimized RVC generator for inference by baking %d weight-normalized layers.",
                            removed_weight_norms,
                        )

                    from rvc.modules.vc.utils import load_hubert
                    vc.hubert_model = load_hubert(vc.config, str(runtime.hubert_path.expanduser().resolve()))
                    logger.info("RVC inference device: %s", vc.config.device)
                except Exception as exc:
                    logger.exception("Unable to load RVC model: %s", model_path)
                    message = f"Unable to load RVC model '{model_path.name}'."
                    self.model_errors[key] = message
                    raise RVCInferenceError(message) from exc
                self.models[key] = vc

            model_lock: threading.Lock = self.model_locks.setdefault(key, threading.Lock())

        with model_lock:
            vc: Any = self.models[key]
            if str(vc.config.device) == "cpu" and options.f0_method != "pm":
                if key not in self.realtime_pitch_warning_models:
                    logger.warning("Using PM pitch tracking for realtime CPU conversion.")
                    self.realtime_pitch_warning_models.add(key)
                options = options.model_copy(update={"f0_method": "pm"})
            output, target_sr = infer_window(vc, audio, options)
            return encode_wav(output, target_sr), target_sr

    def reset(self) -> None:
        with self.lock:
            self.models.clear()
            self.model_errors.clear()
            self.model_locks.clear()
            self.realtime_pitch_warning_models.clear()
            self.vc_class = None
