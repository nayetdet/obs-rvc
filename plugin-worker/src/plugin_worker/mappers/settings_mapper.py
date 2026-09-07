from __future__ import annotations

from ..schemas.responses.settings_response_schema import SettingsResponseSchema
from ..settings import Settings


class SettingsMapper:
    @staticmethod
    def from_success() -> SettingsResponseSchema:
        response: SettingsResponseSchema = SettingsResponseSchema()
        response.status = Settings.status_ok
        return response

    @staticmethod
    def from_error_message(message: str) -> SettingsResponseSchema:
        response: SettingsResponseSchema = SettingsResponseSchema()
        response.status = Settings.status_error
        encoded: bytes = message.encode("utf-8")[: Settings.max_error_bytes - 1]
        response.error = encoded
        response.error_size = len(encoded)
        return response
