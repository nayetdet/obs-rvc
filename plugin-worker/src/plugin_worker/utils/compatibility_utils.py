from __future__ import annotations

import os
from pathlib import Path
from typing import Any


def available_cpu_count() -> int:
    available = os.cpu_count() or 1
    if hasattr(os, "sched_getaffinity"):
        try:
            available = min(available, len(os.sched_getaffinity(0)))
        except OSError:
            pass

    cgroup_v2 = Path("/sys/fs/cgroup/cpu.max")
    try:
        quota, period = cgroup_v2.read_text().split()[:2]
        if quota != "max":
            quota_cpus = max(1, (int(quota) + int(period) - 1) // int(period))
            available = min(available, quota_cpus)
    except (OSError, ValueError, ZeroDivisionError):
        quota_path = Path("/sys/fs/cgroup/cpu/cpu.cfs_quota_us")
        period_path = Path("/sys/fs/cgroup/cpu/cpu.cfs_period_us")
        try:
            quota = int(quota_path.read_text())
            period = int(period_path.read_text())
            if quota > 0 and period > 0:
                available = min(available, max(1, (quota + period - 1) // period))
        except (OSError, ValueError, ZeroDivisionError):
            pass
    return max(1, min(256, available))


def configure_torch(num_threads: int = 1, reserved_threads: int = 0) -> int:
    import torch

    available_cpus = available_cpu_count()
    worker_limit = max(1, available_cpus - max(0, min(available_cpus - 1, int(reserved_threads))))
    effective_threads = min(max(1, min(256, int(num_threads))), worker_limit)
    os.environ["OMP_NUM_THREADS"] = str(effective_threads)
    os.environ["MKL_NUM_THREADS"] = str(effective_threads)
    torch.backends.mkldnn.enabled = True
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
