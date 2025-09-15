#!/bin/bash

# Check if root directory argument is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <root-directory>"
  exit 1
fi

ROOT_DIR="$1"

# Iterate over all subdirectories inside the root directory
for dir in "$ROOT_DIR"/*/; do
  # Check if package.json exists in the subdirectory
  if [ -f "$dir/package.json" ]; then
    echo "Found package.json in $dir"
    echo "Running npm run build..."
    (
      cd "$dir" || exit
      npm run build
    )
  else
    echo "No package.json in $dir, skipping..."
  fi
done
