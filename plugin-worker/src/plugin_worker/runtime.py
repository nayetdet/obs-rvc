from pathlib import Path

from pydantic import BaseModel, ConfigDict


class Runtime(BaseModel):
    model_config = ConfigDict(validate_assignment=True)

    model: str | None = None
    hubert_path: Path | None = None
    rmvpe_root: Path | None = None


runtime: Runtime = Runtime()
