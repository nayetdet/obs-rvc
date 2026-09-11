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
        model_dir: str = TextUtils.decode(request.model_dir)
        hubert_path: str = TextUtils.decode(request.hubert_path)
        rmvpe_root: str = TextUtils.decode(request.rmvpe_root)
        if not model_dir or not hubert_path or not rmvpe_root:
            return SettingsMapper.from_error_message("Worker settings are invalid.")

        runtime.model = TextUtils.decode(request.model) or None
        runtime.model_dir = Path(model_dir)
        runtime.hubert_path = Path(hubert_path)
        runtime.rmvpe_root = Path(rmvpe_root)

        self.rvc.reset()
        return SettingsMapper.from_success()
