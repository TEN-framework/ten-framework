#!/bin/bash

# The folder of this script.
SCRIPT_DIR=$(
  cd "$(dirname "$0")" || exit
  pwd
)

# The folder of the project.
PROJECT_ROOT_DIR=$(
  cd "$SCRIPT_DIR/.." || exit
  pwd
)

# The folder of the `frontend`.
FRONTEND_DIR="$PROJECT_ROOT_DIR/frontend"

usage() {
  echo "Usage: $0 <base-dir>"
  echo "  <base-dir>  Base directory argument for Rust CLI"
  exit 1
}

# Check if 'base-dir' option has provided.
if [[ $# -ne 1 ]]; then
  echo "Error: Missing base directory argument."
  usage
fi

BASE_DIR="$1"

# Change to `frontend` folder and build the frontend.
echo "Building frontend..."
cd "$FRONTEND_DIR" || {
  echo "Error: Cannot change to frontend directory."
  exit 1
}
npm install || {
  echo "Error: npm install failed."
  exit 1
}
npm run build || {
  echo "Error: npm run build failed."
  exit 1
}
echo "Frontend built successfully."

# Change back to the project root folder.
cd "$PROJECT_ROOT_DIR" || {
  echo "Error: Cannot change to project root directory."
  exit 1
}

# Build tman rust project.
echo "Building and running tman designer ..."
cargo run designer --base-dir="$BASE_DIR" || {
  echo "Error: tman failed to run."
  exit 1
}
