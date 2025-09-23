#!/usr/bin/env bash

set -euo pipefail

APP_HOME=$(cd $(dirname $0)/.. && pwd)
cd "$APP_HOME"

# Ensure deps are installed (optional, rely on your existing installer)
# PIP_INSTALL_CMD="uv pip install --system" "$APP_HOME/scripts/install_python_deps.sh"

# Set PYTHONPATH to include the current directory for imports
export PYTHONPATH="$APP_HOME:${PYTHONPATH:-}"

exec python3 -m uvicorn server.main:app --host 0.0.0.0 --port 8080
