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
        "--additional-hooks-dir",
        str(ROOT / "scripts" / "pyinstaller-hooks"),
        "--runtime-hook",
        str(ROOT / "scripts" / "pyinstaller-hooks" / "fairseq-hook.py"),
        "--distpath",
        str(args.output.resolve()),
        "--workpath",
        str(args.output.resolve() / ".build"),
        "--specpath",
        str(args.output.resolve() / ".spec"),
        "--recursive-copy-metadata",
        "rvc",
        "--copy-metadata",
        "pyworld",
    ]

    command.extend(("--collect-all", "fairseq", "--collect-data", "rvc", "--collect-data", "torchcrepe"))
    command.append(str(ROOT / "src" / "plugin_worker" / "__main__.py"))
    subprocess.run(command, check=True)


if __name__ == "__main__":
    main()
