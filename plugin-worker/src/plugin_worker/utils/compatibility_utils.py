from __future__ import annotations

import os
from pathlib import Path
from typing import Any


def physical_cpu_count() -> int:
    allowed_cpus = set(range(os.cpu_count() or 1))
    if hasattr(os, "sched_getaffinity"):
        try:
            allowed_cpus = set(os.sched_getaffinity(0))
        except OSError:
            pass

    topology_root = Path("/sys/devices/system/cpu")
    cores: set[tuple[str, str]] = set()
    for cpu in allowed_cpus:
        topology = topology_root / f"cpu{cpu}" / "topology"
        try:
            cores.add(
                (
                    (topology / "physical_package_id").read_text().strip(),
                    (topology / "core_id").read_text().strip(),
                )
            )
        except OSError:
            continue
    return max(1, len(cores) if cores else len(allowed_cpus))


def configure_torch(num_threads: int = 1) -> int:
    import torch

    physical_cores = physical_cpu_count()
    worker_limit = physical_cores if physical_cores < 4 else physical_cores - 2
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
