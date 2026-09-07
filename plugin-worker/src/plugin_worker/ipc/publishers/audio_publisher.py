from __future__ import annotations

from ...schemas.responses.audio_response_schema import AudioResponseSchema
from .base_publisher import BasePublisher


class AudioPublisher(BasePublisher[AudioResponseSchema]):
    pass
