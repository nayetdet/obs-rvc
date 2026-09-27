from __future__ import annotations

from io import BytesIO
import struct

import numpy as np
import soundfile as sf


def decode_wav(audio: bytes) -> tuple[np.ndarray, int]:
    view = memoryview(audio)
    if len(view) >= 44 and view[:4] == b"RIFF" and view[8:12] == b"WAVE":
        try:
            riff_size = struct.unpack_from("<I", view, 4)[0]
            if 4 <= riff_size <= len(view) - 8:
                offset = 12
                fmt: tuple[int, int, int, int, int, int] | None = None
                data: memoryview | None = None
                riff_end = riff_size + 8
                while offset + 8 <= riff_end:
                    chunk_size = struct.unpack_from("<I", view, offset + 4)[0]
                    offset += 8
                    if chunk_size > riff_end - offset:
                        break
                    chunk_id = view[offset - 8 : offset - 4]
                    if chunk_id == b"fmt " and chunk_size >= 16:
                        fmt = struct.unpack_from("<HHIIHH", view, offset)
                    elif chunk_id == b"data":
                        data = view[offset : offset + chunk_size]
                    offset += chunk_size + (chunk_size & 1)

                if fmt is not None and data is not None:
                    encoding, channels, sample_rate, _, block_align, bits_per_sample = fmt
                    if (encoding == 1 and channels > 0 and bits_per_sample == 16 and
                            block_align == channels * 2 and len(data) % block_align == 0):
                        samples = np.frombuffer(data, dtype="<i2").astype(np.float32)
                        samples *= 1.0 / 32768.0
                        if channels == 1:
                            return samples, sample_rate
                        return samples.reshape(-1, channels).mean(axis=1, dtype=np.float32), sample_rate
        except (struct.error, ValueError):
            pass

    samples, sample_rate = sf.read(BytesIO(audio), dtype="float32", always_2d=True)
    return (samples[:, 0] if samples.shape[1] == 1 else samples.mean(axis=1, dtype=np.float32)), sample_rate


def encode_wav(audio: np.ndarray, sample_rate: int) -> bytes:
    samples = np.asarray(audio)
    if samples.dtype == np.int16:
        pcm = np.ascontiguousarray(samples.astype("<i2", copy=False))
    else:
        normalized = np.clip(np.nan_to_num(samples.astype(np.float32, copy=False)), -1.0, 1.0)
        pcm = np.minimum(np.floor(normalized * 32768.0), 32767.0).astype("<i2")

    data_size = pcm.nbytes
    if sample_rate <= 0 or data_size > 0xFFFFFFFF - 36:
        raise ValueError("Invalid PCM WAV dimensions")

    header = struct.pack(
        "<4sI4s4sIHHIIHH4sI",
        b"RIFF",
        36 + data_size,
        b"WAVE",
        b"fmt ",
        16,
        1,
        1,
        sample_rate,
        sample_rate * 2,
        2,
        16,
        b"data",
        data_size,
    )

    return header + pcm.tobytes()
