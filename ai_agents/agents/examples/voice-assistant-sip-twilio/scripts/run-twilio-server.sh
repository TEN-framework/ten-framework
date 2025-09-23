#!/usr/bin/env bash

set -euo pipefail

APP_HOME=$(cd $(dirname $0)/.. && pwd)
cd "$APP_HOME"

# Set environment variables (if needed)
export PYTHONPATH="$APP_HOME:${PYTHONPATH:-}"

# Start Twilio server
echo "Starting Twilio server..."
exec python3 server/main.py
