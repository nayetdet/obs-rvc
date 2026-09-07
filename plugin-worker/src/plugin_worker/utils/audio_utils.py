from __future__ import annotations

from collections.abc import Generator
from contextlib import contextmanager
from pathlib import Path
from tempfile import TemporaryDirectory


class AudioUtils:
    @staticmethod
    @contextmanager
    def input_file(audio: bytes, input_format: str) -> Generator[Path, None, None]:
        with TemporaryDirectory(prefix="obs-rvc-") as directory:
            input_path: Path = Path(directory) / f"input.{input_format}"
            input_path.write_bytes(audio)
            yield input_path
