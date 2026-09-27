from __future__ import annotations

from pathlib import Path

from ..exceptions.rvc_inference_exceptions import RVCInferenceException

from ..core.rvc.rvc_inference import RVCInference
from ..mappers.settings_mapper import settings_error, settings_success
from ..schemas.requests.settings_request_schema import SettingsRequestSchema
from ..schemas.responses.settings_response_schema import SettingsResponseSchema
from ..runtime import runtime
from ..settings import settings
from ..utils.text_utils import decode_c_string
from .base_endpoint import BaseEndpoint


class SettingsEndpoint(BaseEndpoint[SettingsRequestSchema, SettingsResponseSchema]):
    def __init__(self, rvc: RVCInference) -> None:
        super().__init__(
            f"{settings.service_name}/settings",
            SettingsRequestSchema,
            SettingsResponseSchema,
        )

        self.rvc = rvc

    def handle(self, request: SettingsRequestSchema) -> SettingsResponseSchema:
        hubert_path: str = decode_c_string(request.hubert_path)
        rmvpe_path: str = decode_c_string(request.rmvpe_path)
        if not hubert_path or not rmvpe_path:
            return settings_error("Worker settings are invalid.")

        runtime.model = decode_c_string(request.model) or None
        runtime.hubert_path = Path(hubert_path)
        runtime.rmvpe_path = Path(rmvpe_path)
        runtime.inference_threads = max(1, min(256, int(request.inference_threads)))
        runtime.obs_reserved_threads = max(0, min(256, int(request.obs_reserved_threads)))

        self.rvc.reset()
        try:
            self.rvc.warmup()
        except RVCInferenceException as exc:
            return settings_error(str(exc))
        except Exception:
            return settings_error("Unable to prepare the RVC model.")
        return settings_success(self.rvc.uses_gpu_acceleration())
