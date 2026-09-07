from __future__ import annotations

from ...schemas.responses.settings_response_schema import SettingsResponseSchema
from .base_publisher import BasePublisher


class SettingsPublisher(BasePublisher[SettingsResponseSchema]):
    pass
