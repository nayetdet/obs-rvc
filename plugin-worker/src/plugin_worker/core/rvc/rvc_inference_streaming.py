from __future__ import annotations

from time import perf_counter
from typing import Any

import numpy as np

from ...runtime import Runtime


def infer_window(
    vc: Any, stream: Any, audio: np.ndarray, rate: int, options: Runtime.Options
) -> tuple[np.ndarray, int, dict[str, float]]:
    import torch
    from rvc.modules.vc.pipeline import change_rms, cache_harvest_f0, input_audio_path2wav

    timings: dict[str, float] = {}
    samples = np.asarray(audio, dtype=np.float32) * (1.0 / 32768.0)
    input_overlap = min(len(stream.input_filter.previous_input) if stream.input_filter is not None else 0, rate * 40 // 1000)
    started = perf_counter()
    samples = stream.input_resampler.resample(samples, rate, 16000)
    timings["input_resample"] = perf_counter() - started

    started = perf_counter()
    samples = stream.input_filter.filter(samples)
    timings["filter"] = perf_counter() - started
    peak = float(np.max(np.abs(samples)))
    if peak > 0.95:
        samples *= 0.95 / peak

    pipeline = vc.pipeline
    pitch = pitchf = None
    pitch_key = f"obs-stream-{id(vc)}"
    with torch.inference_mode():
        if vc.if_f0:
            started = perf_counter()
            try:
                coarse, fine = pipeline.get_f0(
                    pitch_key,
                    samples,
                    len(samples) // pipeline.window,
                    options.f0_up_key,
                    options.f0_method,
                    options.filter_radius,
                )
            finally:
                if options.f0_method == "harvest":
                    input_audio_path2wav.pop(pitch_key, None)
                    cache_harvest_f0.cache_clear()
            length = len(samples) // pipeline.window
            pitch, pitchf = pitch_tensors(stream, coarse, fine, length, pipeline.device, torch)
            timings["f0"] = perf_counter() - started
        else:
            timings["f0"] = 0.0

        speaker = vc.speakers.get(options.speaker)
        if speaker is None:
            speaker = torch.tensor([options.speaker], device=pipeline.device, dtype=torch.long)
            vc.speakers[options.speaker] = speaker

        started = perf_counter()
        features = extract_hubert_features(vc, samples, torch)
        timings["hubert"] = perf_counter() - started
        started = perf_counter()
        result = synthesize(vc, features, speaker, samples, pitch, pitchf, options, torch)
        timings["generator"] = perf_counter() - started
        timings["vc_overhead"] = 0.0

    started = perf_counter()
    if options.rms_mix_rate < 1.0:
        result = change_rms(samples, 16000, result, vc.tgt_sr, options.rms_mix_rate)
    timings["rms"] = perf_counter() - started

    target_rate = options.resample_sr if options.resample_sr >= 16000 else vc.tgt_sr
    started = perf_counter()
    if target_rate != vc.tgt_sr:
        output_overlap = round(input_overlap * vc.tgt_sr / rate)
        result = stream.output_resampler.resample(result, vc.tgt_sr, target_rate, output_overlap)
    timings["output_resample"] = perf_counter() - started
    return np.asarray(result, dtype=np.float32), int(target_rate), timings


def extract_hubert_features(vc: Any, samples: np.ndarray, torch: Any) -> Any:
    features = torch.from_numpy(samples)
    features = features.half() if vc.config.is_half else features.float()
    if features.dim() == 2:
        features = features.mean(-1)

    features = features.view(1, -1)
    padding_mask = torch.zeros(features.shape, device=vc.pipeline.device, dtype=torch.bool)
    inputs = {
        "source": features.to(vc.pipeline.device),
        "padding_mask": padding_mask,
        "output_layer": 9 if vc.version == "v1" else 12,
    }

    with vc.hubert_lock:
        with torch.no_grad():
            logits = vc.hubert_model.extract_features(**inputs)
            output = vc.hubert_model.final_proj(logits[0]) if vc.version == "v1" else logits[0]
    return output


def synthesize(
    vc: Any,
    features: Any,
    speaker: Any,
    samples: np.ndarray,
    pitch: Any,
    pitchf: Any,
    options: Runtime.Options,
    torch: Any,
) -> np.ndarray:
    import torch.nn.functional as functional

    if options.protect < 0.5 and pitch is not None and pitchf is not None:
        original_features = features.clone()

    if vc.index is not None and vc.big_npy is not None and options.index_rate != 0.0:
        vectors = features[0].cpu().numpy()
        if vc.config.is_half:
            vectors = vectors.astype("float32")

        score, indices = vc.index.search(vectors, k=8)
        weights = np.square(1.0 / score)
        weights /= weights.sum(axis=1, keepdims=True)
        vectors = np.sum(vc.big_npy[indices] * np.expand_dims(weights, axis=2), axis=1)
        if vc.config.is_half:
            vectors = vectors.astype("float16")

        features = torch.from_numpy(vectors).unsqueeze(0).to(vc.pipeline.device) * options.index_rate + (
            1.0 - options.index_rate
        ) * features

    features = functional.interpolate(features.permute(0, 2, 1), scale_factor=2).permute(0, 2, 1)
    if options.protect < 0.5 and pitch is not None and pitchf is not None:
        original_features = functional.interpolate(original_features.permute(0, 2, 1), scale_factor=2).permute(0, 2, 1)

    length = min(features.shape[1], len(samples) // vc.pipeline.window)
    if pitch is not None and pitchf is not None:
        pitch = pitch[:, :length]
        pitchf = pitchf[:, :length]

    if options.protect < 0.5 and pitch is not None and pitchf is not None:
        protection = pitchf.clone()
        protection[pitchf > 0] = 1.0
        protection[pitchf < 1] = options.protect
        protection = protection.unsqueeze(-1)
        features = features * protection + original_features * (1.0 - protection)
        features = features.to(original_features.dtype)

    length_tensor = torch.tensor([length], device=vc.pipeline.device).long()
    arguments = (features, length_tensor, pitch, pitchf, speaker) if pitch is not None and pitchf is not None else (
        features,
        length_tensor,
        speaker,
    )

    with torch.no_grad():
        output = vc.net_g.infer(*arguments)[0][0, 0].data.cpu().float().numpy()
    return output


def pitch_tensors(
    stream: Any, coarse: np.ndarray, fine: np.ndarray, length: int, device: Any, torch: Any
) -> tuple[Any, Any]:
    key = str(device)
    buffers = stream.pitch_buffers.get(key)
    if buffers is None or buffers[0].shape[1] < length:
        capacity = 1 << max(1, length - 1).bit_length()
        buffers = (
            torch.empty((1, capacity), device=device, dtype=torch.long),
            torch.empty((1, capacity), device=device, dtype=torch.float32),
        )

        stream.pitch_buffers[key] = buffers

    buffers[0][0, :length].copy_(torch.from_numpy(np.ascontiguousarray(coarse[:length])))
    buffers[1][0, :length].copy_(torch.from_numpy(np.ascontiguousarray(fine[:length], dtype=np.float32)))
    return buffers[0][:, :length], buffers[1][:, :length]
