from __future__ import annotations

from pydantic import BaseModel, ConfigDict, Field


class RVCInferenceOptionsSchema(BaseModel):
    model_config = ConfigDict(extra="forbid")

    speaker: int = Field(default=0, ge=0)
    f0_up_key: int = Field(default=0, ge=-36, le=36)
    f0_method: str = "rmvpe"
    index_file: str | None = None
    index_rate: float = Field(default=0.75, ge=0.0, le=1.0)
    filter_radius: int = Field(default=3, ge=0, le=20)
    resample_sr: int = Field(default=0, ge=0, le=192000)
    rms_mix_rate: float = Field(default=0.25, ge=0.0, le=1.0)
    protect: float = Field(default=0.33, ge=0.0, le=0.5)
