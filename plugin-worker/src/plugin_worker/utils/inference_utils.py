from __future__ import annotations

from typing import Any


def bake_weight_norm(generator: Any) -> int:
    import torch

    removed = 0
    for module in generator.modules():
        try:
            torch.nn.utils.remove_weight_norm(module)
            removed += 1
        except (AttributeError, ValueError):
            pass
    return removed
