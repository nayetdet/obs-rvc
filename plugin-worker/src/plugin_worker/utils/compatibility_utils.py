from __future__ import annotations

import os
from typing import Any

from .hardware_utils import available_cpu_count


def configure_torch(num_threads: int = 1, reserved_threads: int = 0) -> int:
    import torch

    available_cpus = available_cpu_count()
    worker_limit = max(1, available_cpus - max(0, min(available_cpus - 1, int(reserved_threads))))
    effective_threads = min(max(1, min(256, int(num_threads))), worker_limit)
    os.environ["OMP_NUM_THREADS"] = str(effective_threads)
    os.environ["MKL_NUM_THREADS"] = str(effective_threads)
    mkldnn = getattr(torch.backends, "mkldnn", None)
    if mkldnn is not None and mkldnn.is_available():
        mkldnn.enabled = True

    torch.set_num_threads(effective_threads)
    try:
        torch.set_num_interop_threads(1)
    except RuntimeError:
        pass
    if getattr(torch.load, "_obs_rvc_compatible", False):
        return effective_threads
    load_checkpoint = torch.load

    def load_checkpoint_compatible(*args: Any, **kwargs: Any) -> Any:
        kwargs.setdefault("weights_only", False)
        return load_checkpoint(*args, **kwargs)

    load_checkpoint_compatible._obs_rvc_compatible = True  # type: ignore[attr-defined]
    torch.load = load_checkpoint_compatible
    return effective_threads
