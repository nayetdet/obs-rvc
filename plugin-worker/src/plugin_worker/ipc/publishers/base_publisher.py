from __future__ import annotations

from typing import Generic, TypeVar

import iceoryx2 as iox2

from ...schemas.responses.base_response_schema import BaseResponseSchema

ResponseSchema = TypeVar("ResponseSchema", bound=BaseResponseSchema)


class BasePublisher(Generic[ResponseSchema]):
    def publish(self, request: iox2.ActiveRequest, response: ResponseSchema) -> None:
        request.send_copy(response)
