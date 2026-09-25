from __future__ import annotations

import logging
import os
import threading
from io import BytesIO
from pathlib import Path
from typing import Any

import numpy as np
import soundfile as sf

from ..exceptions.rvc_inference_error import RVCInferenceError
from ..exceptions.rvc_inference_model_not_found import RVCInferenceModelNotFoundError
from ..schemas.internal.rvc_inference_options_schema import RVCInferenceOptionsSchema
from ..runtime import runtime
from ..utils.audio_utils import AudioUtils

logger = logging.getLogger(__name__)


class RVCInference:
    models: dict[str, Any] = {}
    model_locks: dict[str, threading.Lock] = {}
    lock: threading.Lock = threading.Lock()
    vc_class: Any = None

    def convert(
        self,
        audio: bytes,
        model: str | None,
        input_format: str,
        options: RVCInferenceOptionsSchema,
    ) -> tuple[bytes, int]:
        if not audio:
            raise RVCInferenceError()

        if runtime.hubert_path is None or runtime.rmvpe_path is None:
            raise RVCInferenceError()

        if not model and not runtime.model:
            raise RVCInferenceModelNotFoundError()

        model_path: Path = Path(model or runtime.model or "")
        model_path = model_path.expanduser().resolve()
        if model_path.suffix.lower() != ".pth" or not model_path.is_file():
            raise RVCInferenceModelNotFoundError()

        with self.lock:
            if self.vc_class is None:
                if not runtime.hubert_path.expanduser().resolve().is_file():
                    raise RVCInferenceError()

                rmvpe_path: Path = runtime.rmvpe_path.expanduser().resolve()
                if not rmvpe_path.is_file() or rmvpe_path.name != "rmvpe.pt":
                    raise RVCInferenceError()

                os.environ.update(
                    hubert_path=str(runtime.hubert_path.expanduser().resolve()),
                    rmvpe_root=str(rmvpe_path.parent),
                    weight_root=str(model_path.parent),
                )

                from rvc.modules.vc.modules import VC
                self.vc_class = VC

            key: str = str(model_path)
            if key not in self.models:
                logger.info("Loading RVC model: %s", model_path)
                try:
                    vc: Any = self.vc_class()
                    vc.get_vc(key)
                except Exception as exc:
                    logger.exception("Unable to load RVC model: %s", model_path)
                    raise RVCInferenceError(f"Unable to load RVC model '{model_path.name}'.") from exc
                self.models[key] = vc

            model_lock: threading.Lock = self.model_locks.setdefault(key, threading.Lock())

        with model_lock:
            with AudioUtils.input_file(audio, input_format) as input_path:
                vc: Any = self.models[key]
                target_sr: int | None
                output: Any
                error: Any
                target_sr, output, _, error = vc.vc_inference(
                    options.speaker,
                    input_path,
                    options.f0_up_key,
                    options.f0_method,
                    filter_radius=options.filter_radius,
                    resample_sr=options.resample_sr,
                    rms_mix_rate=options.rms_mix_rate,
                    protect=options.protect,
                    hubert_path=runtime.hubert_path,
                )

                if error or output is None or target_sr is None:
                    raise RVCInferenceError()

                buffer: BytesIO = BytesIO()
                sf.write(buffer, np.asarray(output, dtype=np.float32), target_sr, format="WAV", subtype="PCM_16")
                return buffer.getvalue(), int(target_sr)

    def reset(self) -> None:
        with self.lock:
            self.models.clear()
            self.model_locks.clear()
            self.vc_class = None
