from __future__ import annotations

from io import BytesIO
from typing import Any

import numpy as np
import soundfile as sf

from ..schemas.internal.rvc_inference_options_schema import RVCInferenceOptionsSchema


def infer_window(vc: Any, audio: bytes, options: RVCInferenceOptionsSchema) -> tuple[np.ndarray, int]:
    import librosa
    import torch
    from scipy.signal import filtfilt
    from rvc.modules.vc.pipeline import bh, ah, change_rms, cache_harvest_f0, input_audio_path2wav

    samples, rate = sf.read(BytesIO(audio), dtype="float32", always_2d=True)
    samples = samples.mean(axis=1)
    if rate != 16000:
        samples = librosa.resample(samples, orig_sr=rate, target_sr=16000)

    samples = np.asarray(filtfilt(bh, ah, samples), dtype=np.float32)
    peak = float(np.max(np.abs(samples)))
    if peak > 0.95:
        samples *= 0.95 / peak

    pipeline = vc.pipeline
    times = {"npy": 0.0, "f0": 0.0, "infer": 0.0}
    pitch = pitchf = None
    pitch_key = f"obs-stream-{id(vc)}"
    with torch.inference_mode():
        if vc.if_f0:
            try:
                coarse, fine = pipeline.get_f0(
                    pitch_key, samples, len(samples) // pipeline.window,
                    options.f0_up_key, options.f0_method, options.filter_radius,
                )
            finally:
                if options.f0_method == "harvest":
                    input_audio_path2wav.pop(pitch_key, None)
                    cache_harvest_f0.cache_clear()
            length = len(samples) // pipeline.window
            pitch = torch.as_tensor(coarse[:length], device=pipeline.device).unsqueeze(0).long()
            pitchf = torch.as_tensor(fine[:length], device=pipeline.device).unsqueeze(0).float()

        speaker = torch.tensor([options.speaker], device=pipeline.device, dtype=torch.long)
        result = pipeline.vc(
            vc.hubert_model, vc.net_g, speaker, samples, pitch, pitchf, times,
            getattr(vc, "index_path", None), None, options.index_rate, vc.version, options.protect,
        )

    if options.rms_mix_rate < 1.0:
        result = change_rms(samples, 16000, result, vc.tgt_sr, options.rms_mix_rate)

    target_rate = options.resample_sr if options.resample_sr >= 16000 else vc.tgt_sr
    if target_rate != vc.tgt_sr:
        result = librosa.resample(result, orig_sr=vc.tgt_sr, target_sr=target_rate)
    return np.asarray(result, dtype=np.float32), int(target_rate)
