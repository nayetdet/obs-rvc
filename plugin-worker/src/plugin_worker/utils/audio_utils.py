from __future__ import annotations

from collections.abc import Generator
from contextlib import contextmanager
from pathlib import Path
from tempfile import NamedTemporaryFile


class AudioUtils:
    @staticmethod
    @contextmanager
    def input_file(audio: bytes, input_format: str) -> Generator[Path, None, None]:
        with NamedTemporaryFile(suffix=f".{Path(input_format).name}") as file:
            file.write(audio)
            file.flush()
            yield Path(file.name)
