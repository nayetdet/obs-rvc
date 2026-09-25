from typing import ClassVar


class RVCInferenceError(RuntimeError):
    message: ClassVar[str] = "RVC inference failed."

    def __init__(self, message: str | None = None) -> None:
        super().__init__(message or self.message)
