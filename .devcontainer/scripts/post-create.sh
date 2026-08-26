#!/usr/bin/env bash
set -euo pipefail

bun add --global @openai/codex

cmake --preset ubuntu-x86_64
