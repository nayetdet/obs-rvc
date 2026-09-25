from __future__ import annotations

from pathlib import Path

from ...core.rvc_inference import RVCInference
from ...mappers.settings_mapper import SettingsMapper
from ...schemas.requests.settings_request_schema import SettingsRequestSchema
from ...schemas.responses.settings_response_schema import SettingsResponseSchema
from ...runtime import runtime
from ...settings import settings
from ...utils.text_utils import TextUtils
from .base_handler import BaseHandler
from ..publishers.settings_publisher import SettingsPublisher
from ..subscribers.settings_subscriber import SettingsSubscriber


class SettingsHandler(BaseHandler[SettingsRequestSchema, SettingsResponseSchema]):
    def __init__(self, rvc: RVCInference) -> None:
        super().__init__(
            f"{settings.service_name}/settings",
            SettingsRequestSchema,
            SettingsResponseSchema,
            SettingsPublisher(),
            SettingsSubscriber(),
        )

        self.rvc = rvc

    def handle(self, request: SettingsRequestSchema) -> SettingsResponseSchema:
        hubert_path: str = TextUtils.decode(request.hubert_path)
        rmvpe_path: str = TextUtils.decode(request.rmvpe_path)
        if not hubert_path or not rmvpe_path:
            return SettingsMapper.from_error_message("Worker settings are invalid.")

        runtime.model = TextUtils.decode(request.model) or None
        runtime.hubert_path = Path(hubert_path)
        runtime.rmvpe_path = Path(rmvpe_path)

        self.rvc.reset()
        return SettingsMapper.from_success()
