import os
from pathlib import Path

from pydantic import BaseModel, ConfigDict


class Runtime(BaseModel):
    model_config = ConfigDict(validate_assignment=True)

    model: str | None = None
    hubert_path: Path | None = None
    rmvpe_path: Path | None = None
    inference_threads: int = max(1, min(256, os.cpu_count() or 1))
    obs_reserved_threads: int = 0


runtime: Runtime = Runtime()
