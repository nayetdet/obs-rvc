from typing import ClassVar


class RVCInferenceError(RuntimeError):
    message: ClassVar[str] = "RVC inference failed."

    def __init__(self) -> None:
        super().__init__(self.message)
