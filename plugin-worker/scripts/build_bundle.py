from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description="Bundle the OBS RVC worker.")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    command = [
        sys.executable,
        "-m",
        "PyInstaller",
        "--noconfirm",
        "--clean",
        "--onedir",
        "--name",
        "obs-rvc-worker",
        "--paths",
        str(ROOT / "src"),
        "--distpath",
        str(args.output.resolve()),
        "--workpath",
        str(args.output.resolve() / ".build"),
        "--specpath",
        str(args.output.resolve() / ".spec"),
        "--recursive-copy-metadata",
        "rvc",
    ]

    for package in (
        "iceoryx2",
        "rvc",
        "torch",
        "torchaudio",
        "torchcrepe",
        "fairseq",
        "librosa",
        "soundfile",
    ):
        command.extend(("--collect-all", package))

    command.append(str(ROOT / "src" / "plugin_worker" / "__main__.py"))
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
