from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Generic, TypeVar

import iceoryx2 as iox2

from ..schemas.requests.base_request_schema import BaseRequestSchema
from ..schemas.responses.base_response_schema import BaseResponseSchema

RequestSchema = TypeVar("RequestSchema", bound=BaseRequestSchema)
ResponseSchema = TypeVar("ResponseSchema", bound=BaseResponseSchema)


class BaseEndpoint(Generic[RequestSchema, ResponseSchema], ABC):
    def __init__(
        self,
        service_name: str,
        request_schema: type[RequestSchema],
        response_schema: type[ResponseSchema],
    ) -> None:
        self.service_name: str = service_name
        self.request_schema: type[RequestSchema] = request_schema
        self.response_schema: type[ResponseSchema] = response_schema

    def register(self, node: iox2.Node) -> iox2.Server:
        return (
            node.service_builder(iox2.ServiceName.new(self.service_name))
            .request_response(self.request_schema, self.response_schema)
            .open_or_create()
            .server_builder()
            .create()
        )

    def process(self, server: iox2.Server) -> bool:
        request: iox2.ActiveRequest | None = server.receive()
        if request is None:
            return False
        try:
            response: ResponseSchema = self.handle(request.payload().contents)
            request.send_copy(response)
        finally:
            request.delete()
        return True

    @abstractmethod
    def handle(self, request: RequestSchema) -> ResponseSchema:
        ...
