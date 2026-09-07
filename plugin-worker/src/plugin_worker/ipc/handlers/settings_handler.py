from __future__ import annotations

from pathlib import Path

from pydantic import ValidationError

from ...core.rvc_inference import RVCInference
from ...mappers.settings_mapper import SettingsMapper
from ...schemas.requests.settings_request_schema import SettingsRequestSchema
from ...schemas.responses.settings_response_schema import SettingsResponseSchema
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
        try:
            model: str | None = TextUtils.decode(request.model) or settings.model
            model_dir: str = TextUtils.decode(request.model_dir)
            hubert_path: str = TextUtils.decode(request.hubert_path)
            rmvpe_root: str = TextUtils.decode(request.rmvpe_root)

            settings.model = model
            settings.model_dir = Path(model_dir) if model_dir else settings.model_dir
            settings.hubert_path = Path(hubert_path) if hubert_path else settings.hubert_path
            settings.rmvpe_root = Path(rmvpe_root) if rmvpe_root else settings.rmvpe_root

            self.rvc.reset()
            return SettingsMapper.from_success()
        except ValidationError:
            return SettingsMapper.from_error_message("Worker settings are invalid.")
