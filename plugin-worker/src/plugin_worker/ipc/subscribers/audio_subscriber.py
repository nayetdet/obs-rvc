from __future__ import annotations

import iceoryx2 as iox2


class AudioSubscriber:
    @staticmethod
    def receive(server: iox2.Server) -> iox2.ActiveRequest | None:
        return server.receive()
