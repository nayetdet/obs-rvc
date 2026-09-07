from __future__ import annotations

import iceoryx2 as iox2

from ...schemas.responses.audio_response_schema import AudioResponseSchema


class AudioPublisher:
    @staticmethod
    def publish(request: iox2.ActiveRequest, response: AudioResponseSchema) -> None:
        request.send_copy(response)
