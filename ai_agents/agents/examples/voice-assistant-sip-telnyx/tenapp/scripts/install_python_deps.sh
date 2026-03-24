#!/bin/bash

# Install Python dependencies for the extension

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXTENSION_DIR="$(dirname "$(dirname "$SCRIPT_DIR")")"
EXTENSION_PYTHON_DIR="$EXTENSION_DIR/main_python"

echo "Installing Python dependencies for extension..."
echo "Extension directory: $EXTENSION_PYTHON_DIR"

# Check if requirements.txt exists
if [ -f "$EXTENSION_PYTHON_DIR/requirements.txt" ]; then
    pip install -q -r "$EXTENSION_PYTHON_DIR/requirements.txt"
    echo "Python dependencies installed successfully."
else
    echo "Warning: requirements.txt not found in $EXTENSION_PYTHON_DIR"
fi