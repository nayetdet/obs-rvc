from typing import ClassVar

from .rvc_inference_error import RVCInferenceError


class RVCInferenceModelNotFoundError(RVCInferenceError):
    message: ClassVar[str] = "RVC inference model was not found."
