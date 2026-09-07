from __future__ import annotations

from typing import Generic, TypeVar

import iceoryx2 as iox2

from ...schemas.requests.base_request_schema import BaseRequestSchema

RequestSchema = TypeVar("RequestSchema", bound=BaseRequestSchema)


class BaseSubscriber(Generic[RequestSchema]):
    def receive(self, server: iox2.Server) -> iox2.ActiveRequest | None:
        return server.receive()

    def contents(self, request: iox2.ActiveRequest) -> RequestSchema:
        return request.payload().contents
