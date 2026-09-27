from __future__ import annotations

import os
from pathlib import Path
from typing import Any


def is_gpu_accelerated_device(device: Any) -> bool:
    return str(device).lower() != "cpu"


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
