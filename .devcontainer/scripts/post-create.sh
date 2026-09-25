#!/usr/bin/env bash
set -euo pipefail

bun add --global @openai/codex

python3 -m pip install --user clang-format==19.1.1

cmake -S plugin --preset ubuntu-x86_64
