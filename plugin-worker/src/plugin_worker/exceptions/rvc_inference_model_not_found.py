from typing import ClassVar


class RVCInferenceModelNotFoundError(RuntimeError):
    message: ClassVar[str] = "RVC inference model was not found"

    def __init__(self) -> None:
        super().__init__(self.message)
