from __future__ import annotations

from .base_subscriber import BaseSubscriber
from ...schemas.requests.settings_request_schema import SettingsRequestSchema


class SettingsSubscriber(BaseSubscriber[SettingsRequestSchema]):
    pass
