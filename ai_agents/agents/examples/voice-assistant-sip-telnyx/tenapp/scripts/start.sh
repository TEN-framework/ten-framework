#!/bin/bash

# Start script for Telnyx voice assistant

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TENAPP_DIR="$(dirname "$SCRIPT_DIR")"
cd "$TENAPP_DIR"

echo "Starting Telnyx voice assistant..."
echo "Working directory: $TENAPP_DIR"

# Check if main binary exists
if [ -f "./main" ]; then
    echo "Running main binary..."
    ./main
else
    echo "Main binary not found, looking for alternative..."
    # Try to find and run the main binary
    if [ -f "./go_build/main" ]; then
        ./go_build/main
    else
        echo "Error: main binary not found. Please build the project first."
        exit 1
    fi
fi