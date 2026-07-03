#!/usr/bin/env bash

# Set default npm command
NPM_CMD=${NPM_CMD:-"npm"}

build_nodejs_project() {
  local dir=$1
  local name=$2

  if [[ -f "$dir/package.json" ]]; then
    echo "Building $name..."
    cd "$dir"

    # Check if build script exists
    if $NPM_CMD run build --dry-run >/dev/null 2>&1; then
      echo "Running build for $name..."
      $NPM_CMD run build || {
        echo "Error: Failed to build $name"
        exit 1
      }
      echo "Successfully built $name"
    else
      echo "No build script found for $name, skipping build step"
    fi
  else
    echo "No package.json found in $dir, skipping $name"
  fi
}

main() {
  # Get the parent directory of script location as app root directory
  APP_HOME=$(cd $(dirname $0)/.. && pwd)

  echo "App root directory: $APP_HOME"
  echo "Using npm command: $NPM_CMD"

  # Check if manifest.json exists
  if [[ ! -f "$APP_HOME/manifest.json" ]]; then
    echo "Error: manifest.json file not found"
    exit 1
  fi

  echo "=== Building Node.js Main App ==="

  # Build main app
  build_nodejs_project "$APP_HOME" "main app"

  echo "=== Building Node.js Extensions ==="

  # Build Node.js extensions (those with package.json)
  for dir in "$APP_HOME/ten_packages/extension"/*; do
    if [[ -d "$dir" && -f "$dir/package.json" ]]; then
      build_nodejs_project "$dir" "$(basename "$dir")"
    fi
  done

  echo "All Node.js projects built successfully!"
}

# If script is executed directly, run main function
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  main "$@"
fi
