#!/usr/bin/env bash
set -euo pipefail

mkdir -p /home/vscode/.local/share/zsh

bun add --global @openai/codex

cmake -S plugin --preset ubuntu-x86_64
