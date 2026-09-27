from typing import ClassVar


class RVCInferenceException(RuntimeError):
    message: ClassVar[str] = "RVC inference failed."

    def __init__(self, message: str | None = None) -> None:
        super().__init__(message or self.message)


class RVCInferenceModelNotFoundException(RVCInferenceException):
    message: ClassVar[str] = "RVC inference model was not found."


class RVCAudioException(RVCInferenceException):
    message: ClassVar[str] = "Audio input is empty or has an invalid sample rate."


class RVCConfigurationException(RVCInferenceException):
    message: ClassVar[str] = "RVC inference configuration is incomplete or invalid."
