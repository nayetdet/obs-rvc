from __future__ import annotations

import numpy as np
import soxr


class StreamingResampler:
    def __init__(self) -> None:
        self.input_rate = 0
        self.output_rate = 0
        self.stream: object | None = None
        self.previous_input = np.empty(0, dtype=np.float32)
        self.previous_output = np.empty(0, dtype=np.float32)

    def resample(self, samples: np.ndarray, input_rate: int, output_rate: int) -> np.ndarray:
        if input_rate == output_rate:
            return np.asarray(samples, dtype=np.float32)

        samples = np.ascontiguousarray(samples, dtype=np.float32)
        if input_rate != self.input_rate or output_rate != self.output_rate or self.stream is None:
            self.input_rate = input_rate
            self.output_rate = output_rate
            self.stream = soxr.ResampleStream(input_rate, output_rate, 1, dtype="float32", quality="HQ")
            self.previous_input = np.empty(0, dtype=np.float32)
            self.previous_output = np.empty(0, dtype=np.float32)

        overlap = min(len(self.previous_input), len(samples), input_rate // 10)
        if overlap and np.array_equal(self.previous_input[-overlap:], samples[:overlap]):
            prefix = self.previous_output[-round(overlap * output_rate / input_rate) :]
            fresh = samples[overlap:]
        else:
            self.stream = soxr.ResampleStream(input_rate, output_rate, 1, dtype="float32", quality="HQ")
            prefix = np.empty(0, dtype=np.float32)
            fresh = samples

        output = self.stream.resample_chunk(fresh)
        result = np.concatenate((prefix, output)) if len(prefix) else output
        expected_size = round(len(samples) * output_rate / input_rate)
        if len(result) < expected_size:
            result = np.pad(result, (0, expected_size - len(result)), mode="edge") if len(result) else np.zeros(
                expected_size, dtype=np.float32
            )
        elif len(result) > expected_size:
            result = result[:expected_size]

        self.previous_input = samples.copy()
        self.previous_output = result.copy()
        return result

    def reset(self) -> None:
        self.input_rate = 0
        self.output_rate = 0
        self.stream = None
        self.previous_input = np.empty(0, dtype=np.float32)
        self.previous_output = np.empty(0, dtype=np.float32)
