#!/usr/bin/env bash
set -euo pipefail

git lfs install --local --skip-repo
git lfs pull

bun add --global @openai/codex

python3 -m pip install --user clang-format==19.1.1

cmake -S plugin --preset ubuntu-x86_64
