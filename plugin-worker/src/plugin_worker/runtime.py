from __future__ import annotations

from pathlib import Path
from threading import RLock

from pydantic import BaseModel, ConfigDict, Field, PrivateAttr

from .utils.hardware_utils import available_cpu_count


class Runtime(BaseModel):
    model_config = ConfigDict(validate_assignment=True)

    _lock: RLock = PrivateAttr(default_factory=RLock)

    model: Path | None = None
    hubert_path: Path | None = None
    rmvpe_path: Path | None = None
    inference_threads: int = Field(default_factory=available_cpu_count, ge=1, le=256)
    obs_reserved_threads: int = Field(default=0, ge=0, le=256)

    def snapshot(self) -> Runtime:
        with self._lock:
            return self.model_copy()

    def configure(
        self,
        *,
        model: str | Path | None,
        hubert_path: str | Path,
        rmvpe_path: str | Path,
        inference_threads: int,
        obs_reserved_threads: int,
    ) -> None:
        configuration = type(self)(
            model=model,
            hubert_path=hubert_path,
            rmvpe_path=rmvpe_path,
            inference_threads=max(1, min(256, int(inference_threads))),
            obs_reserved_threads=max(0, min(256, int(obs_reserved_threads))),
        )

        with self._lock:
            self.__dict__.update(configuration.__dict__)


runtime = Runtime()
