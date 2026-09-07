from __future__ import annotations

from .base_subscriber import BaseSubscriber
from ...schemas.requests.audio_request_schema import AudioRequestSchema


class AudioSubscriber(BaseSubscriber[AudioRequestSchema]):
    pass
