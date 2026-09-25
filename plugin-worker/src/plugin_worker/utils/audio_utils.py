from __future__ import annotations

from collections.abc import Generator
from contextlib import contextmanager
from pathlib import Path
from tempfile import NamedTemporaryFile

import numpy as np
import soundfile as sf


class AudioUtils:
    @staticmethod
    def load_audio(path: str, sample_rate: int) -> np.ndarray:
        audio: np.ndarray
        input_sample_rate: int
        audio, input_sample_rate = sf.read(path, dtype="float32", always_2d=True)
        audio = np.mean(audio, axis=1, dtype=np.float32)
        if input_sample_rate != sample_rate:
            import librosa

            audio = librosa.resample(audio, orig_sr=input_sample_rate, target_sr=sample_rate)
        return np.asarray(audio, dtype=np.float32)

    @staticmethod
    @contextmanager
    def input_file(audio: bytes, input_format: str) -> Generator[Path, None, None]:
        with NamedTemporaryFile(suffix=f".{Path(input_format).name}") as file:
            file.write(audio)
            file.flush()
            yield Path(file.name)
