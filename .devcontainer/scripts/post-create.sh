#!/usr/bin/env bash
set -euo pipefail

bun add --global @openai/codex

cmake -S plugin --preset ubuntu-x86_64
