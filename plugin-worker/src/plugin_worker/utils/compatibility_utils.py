from __future__ import annotations

from typing import Any


class CompatibilityUtils:
    @staticmethod
    def configure_torch() -> None:
        import torch

        if getattr(torch.load, "_obs_rvc_compatible", False):
            return

        load_checkpoint = torch.load

        def load_checkpoint_compatible(*args: Any, **kwargs: Any) -> Any:
            kwargs.setdefault("weights_only", False)
            return load_checkpoint(*args, **kwargs)

        load_checkpoint_compatible._obs_rvc_compatible = True  # type: ignore[attr-defined]
        torch.load = load_checkpoint_compatible
