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
from ..settings import settings
from ..utils.audio_utils import AudioUtils

logger = logging.getLogger(__name__)


class RVCInference:
    models: dict[str, Any] = {}
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

        model_dir: Path = settings.model_dir.expanduser().resolve()
        if not model and not settings.model:
            raise RVCInferenceModelNotFoundError()

        model_path: Path = Path(model or settings.model or "")
        model_path = model_path.resolve() if model_path.is_absolute() else (model_dir / model_path).resolve()
        try:
            model_path.relative_to(model_dir)
        except ValueError as exc:
            raise RVCInferenceModelNotFoundError() from exc

        if model_path.suffix.lower() != ".pth" or not model_path.is_file():
            raise RVCInferenceModelNotFoundError()

        with self.lock:
            if self.vc_class is None:
                if settings.hubert_path is None or not settings.hubert_path.expanduser().resolve().is_file():
                    raise RVCInferenceError()

                if settings.rmvpe_root is None:
                    raise RVCInferenceError()

                rmvpe_root: Path = settings.rmvpe_root.expanduser().resolve()
                if not (rmvpe_root / "rmvpe.pt").is_file():
                    raise RVCInferenceError()

                os.environ.update(
                    hubert_path=str(settings.hubert_path.expanduser().resolve()),
                    rmvpe_root=str(rmvpe_root),
                    weight_root=str(model_dir),
                )

                from rvc.modules.vc.modules import VC
                self.vc_class = VC

            key: str = str(model_path)
            if key not in self.models:
                logger.info("Loading RVC model: %s", model_path)
                self.models[key] = self.vc_class()
                self.models[key].get_vc(key)

            with AudioUtils.input_file(audio, input_format) as input_path:
                index_path: Path | None = (
                    Path(options.index_file).expanduser().resolve() if options.index_file else None
                )

                vc: Any = self.models[key]
                target_sr, output, _, error = vc.vc_inference(
                    options.speaker,
                    input_path,
                    options.f0_up_key,
                    options.f0_method,
                    index_file=str(index_path) if index_path and index_path.is_file() else None,
                    index_rate=options.index_rate,
                    filter_radius=options.filter_radius,
                    resample_sr=options.resample_sr,
                    rms_mix_rate=options.rms_mix_rate,
                    protect=options.protect,
                    hubert_path=settings.hubert_path,
                )

                if error or output is None or target_sr is None:
                    raise RVCInferenceError()

                buffer = BytesIO()
                sf.write(buffer, np.asarray(output, dtype=np.float32), target_sr, format="WAV", subtype="PCM_16")
                return buffer.getvalue(), int(target_sr)
