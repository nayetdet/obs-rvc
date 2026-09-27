from typing import Any

from pydantic import BaseModel, ConfigDict, Field

from ...core.streaming.streaming_resampler import StreamingResampler


class AudioStreamSchema(BaseModel):
    model_config = ConfigDict(arbitrary_types_allowed=True)

    generation: int
    input_resampler: StreamingResampler = Field(default_factory=StreamingResampler)
    output_resampler: StreamingResampler = Field(default_factory=StreamingResampler)
    input_filter: Any = None
    pitch_buffer: tuple[Any, Any] | None = None
