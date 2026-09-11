from __future__ import annotations

from ..schemas.responses.settings_response_schema import SettingsResponseSchema
from ..enums.ipc_response_status_enum import IPCResponseStatusEnum
from ..settings import settings


class SettingsMapper:
    @staticmethod
    def from_success() -> SettingsResponseSchema:
        response: SettingsResponseSchema = SettingsResponseSchema()
        response.status = IPCResponseStatusEnum.OK
        return response

    @staticmethod
    def from_error_message(message: str) -> SettingsResponseSchema:
        response: SettingsResponseSchema = SettingsResponseSchema()
        response.status = IPCResponseStatusEnum.ERROR
        encoded: bytes = message.encode("utf-8")[: settings.max_error_bytes - 1]
        response.error = encoded
        response.error_size = len(encoded)
        return response
