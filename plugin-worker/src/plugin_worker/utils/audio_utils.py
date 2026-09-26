from __future__ import annotations

from io import BytesIO

import numpy as np
import soundfile as sf


def encode_wav(audio: np.ndarray, sample_rate: int) -> bytes:
    samples = np.asarray(audio)
    if samples.dtype != np.int16:
        samples = np.clip(np.nan_to_num(samples.astype(np.float32)), -1.0, 1.0)
    buffer = BytesIO()
    sf.write(buffer, samples, sample_rate, format="WAV", subtype="PCM_16")
    return buffer.getvalue()
