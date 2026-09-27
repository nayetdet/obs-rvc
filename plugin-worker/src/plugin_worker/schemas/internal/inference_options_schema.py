from pydantic import BaseModel, ConfigDict, Field


class InferenceOptionsSchema(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True)

    speaker: int = Field(default=0, ge=0)
    f0_up_key: int = Field(default=0, ge=-36, le=36)
    f0_method: str = Field(default="rmvpe", min_length=1, max_length=32)
    filter_radius: int = Field(default=3, ge=0, le=20)
    resample_sr: int = Field(default=0, ge=0, le=192000)
    rms_mix_rate: float = Field(default=1.0, ge=0.0, le=1.0)
    protect: float = Field(default=0.33, ge=0.0, le=0.5)
    index_rate: float = Field(default=0.75, ge=0.0, le=1.0)
