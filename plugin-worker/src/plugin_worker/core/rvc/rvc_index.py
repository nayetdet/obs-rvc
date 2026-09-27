from __future__ import annotations

from typing import Any

import numpy as np

from ...exceptions.rvc_inference_exceptions import RVCIndexException


def load_rvc_index(path: str, version: str) -> tuple[Any, np.ndarray]:
    import faiss

    try:
        index = faiss.read_index(path)
        if index.ntotal <= 0:
            raise RVCIndexException("RVC index is empty.")
        expected_dimension = 256 if version == "v1" else 768
        if index.d != expected_dimension:
            raise RVCIndexException(f"RVC index has {index.d} dimensions; expected {expected_dimension}.")
        vectors = index.reconstruct_n(0, index.ntotal)
        if vectors.shape != (index.ntotal, index.d):
            raise RVCIndexException("RVC index reconstruction has an invalid shape.")
        return index, np.ascontiguousarray(vectors, dtype=np.float32)
    except RVCIndexException:
        raise
    except Exception as exc:
        raise RVCIndexException(f"Unable to load RVC index '{path}'.") from exc
