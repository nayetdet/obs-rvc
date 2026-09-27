from __future__ import annotations

from ..enums.ipc_response_status_enum import IPCResponseStatusEnum
from ..schemas.responses.settings_response_schema import SettingsResponseSchema
from ..settings import Settings


def settings_error(message: str) -> SettingsResponseSchema:
    response = SettingsResponseSchema()
    response.status = IPCResponseStatusEnum.ERROR
    encoded = message.encode("utf-8")[: Settings.max_error_bytes - 1]
    response.error = encoded
    response.error_size = len(encoded)
    return response


def settings_success() -> SettingsResponseSchema:
    response = SettingsResponseSchema()
    response.status = IPCResponseStatusEnum.OK
    return response
