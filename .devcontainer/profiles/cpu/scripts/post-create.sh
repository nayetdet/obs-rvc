#!/usr/bin/env bash
set -euo pipefail

git lfs install --local --skip-repo
git lfs pull

bun add --global @openai/codex

cmake -S plugin --preset ubuntu-x86_64
