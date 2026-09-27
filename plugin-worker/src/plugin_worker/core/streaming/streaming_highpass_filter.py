from __future__ import annotations

import numpy as np
from scipy.signal import sosfilt, sosfilt_zi, tf2sos


class StreamingHighpassFilter:
    def __init__(self, numerator: np.ndarray, denominator: np.ndarray) -> None:
        self.sos = tf2sos(numerator, denominator)
        self.state: np.ndarray | None = None
        self.previous_input = np.empty(0, dtype=np.float32)
        self.previous_output = np.empty(0, dtype=np.float32)

    def filter(self, samples: np.ndarray) -> np.ndarray:
        samples = np.ascontiguousarray(samples, dtype=np.float32)
        overlap = min(len(self.previous_input), len(samples), 1600)
        if overlap and np.array_equal(self.previous_input[-overlap:], samples[:overlap]):
            prefix = self.previous_output[-overlap:]
            fresh = samples[overlap:]
        else:
            self.state = None
            prefix = np.empty(0, dtype=np.float32)
            fresh = samples

        if len(fresh):
            if self.state is None:
                self.state = sosfilt_zi(self.sos) * fresh[0]
            output, self.state = sosfilt(self.sos, fresh, zi=self.state)
            result = np.concatenate((prefix, output)) if len(prefix) else output
        else:
            result = prefix

        self.previous_input = samples.copy()
        self.previous_output = np.asarray(result, dtype=np.float32).copy()
        return self.previous_output
