#!/usr/bin/env bash
set -euo pipefail

tool_bin_dir="${HOME}/.local/bin"
mkdir -p "${tool_bin_dir}"

UV_TOOL_BIN_DIR="${tool_bin_dir}" uv tool install --upgrade 'clang-format==19.1.1'
UV_TOOL_BIN_DIR="${tool_bin_dir}" uv tool install --upgrade 'gersemi>=0.12.0'

export PATH="${tool_bin_dir}:${PATH}"
clang-format --version
gersemi --version
