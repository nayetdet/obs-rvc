from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from threading import Lock
from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from .core.streaming.streaming_resampler import StreamingResampler
from .utils.compatibility_utils import available_cpu_count


class Runtime:
    class Options(BaseModel):
        model_config = ConfigDict(extra="forbid")

        speaker: int = Field(default=0, ge=0)
        f0_up_key: int = Field(default=0, ge=-36, le=36)
        f0_method: str = "rmvpe"
        filter_radius: int = Field(default=3, ge=0, le=20)
        resample_sr: int = Field(default=0, ge=0, le=192000)
        rms_mix_rate: float = Field(default=1.0, ge=0.0, le=1.0)
        protect: float = Field(default=0.33, ge=0.0, le=0.5)
        index_rate: float = Field(default=0.75, ge=0.0, le=1.0)

    @dataclass
    class Stream:
        generation: int
        input_resampler: StreamingResampler = field(default_factory=StreamingResampler)
        output_resampler: StreamingResampler = field(default_factory=StreamingResampler)
        input_filter: Any = None
        pitch_buffer: tuple[Any, Any] | None = None

    @dataclass
    class Hubert:
        model: Any
        lock: Lock = field(default_factory=Lock)

    @dataclass
    class State:
        model: str | None = None
        hubert_path: Path | None = None
        rmvpe_path: Path | None = None
        inference_threads: int = field(default_factory=available_cpu_count)
        obs_reserved_threads: int = 0


runtime: Runtime.State = Runtime.State()
