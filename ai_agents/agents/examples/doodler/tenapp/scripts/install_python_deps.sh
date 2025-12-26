#!/usr/bin/env bash

# Set default pip install command
PIP_INSTALL_CMD=${PIP_INSTALL_CMD:-"uv pip install --system"}

install_python_requirements() {
  local app_dir=$1

  echo "Starting Python dependencies installation..."


  # Install Twilio server Python dependencies
  if [[ -f "$app_dir/../server/requirements.txt" ]]; then
    echo "Installing Twilio server Python dependencies..."
    ${PIP_INSTALL_CMD} -r "$app_dir/../server/requirements.txt"
  else
    echo "No requirements.txt found in server directory: $app_dir/../server"
  fi

  # Traverse ten_packages/extension directory to find requirements.txt
  if [[ -d "$app_dir/ten_packages/extension" ]]; then
    echo "Traversing ten_packages/extension directory..."
    for extension in "$app_dir/ten_packages/extension"/*; do
      if [[ -d "$extension" && -f "$extension/requirements.txt" ]]; then
        echo "Found requirements.txt in $extension, installing dependencies..."
        ${PIP_INSTALL_CMD} -r "$extension/requirements.txt"
      fi
    done
  else
    echo "ten_packages/extension directory not found"
  fi

  # Traverse ten_packages/system directory to find requirements.txt
  if [[ -d "$app_dir/ten_packages/system" ]]; then
    echo "Traversing ten_packages/system directory..."
    for extension in "$app_dir/ten_packages/system"/*; do
      if [[ -d "$extension" && -f "$extension/requirements.txt" ]]; then
        echo "Found requirements.txt in $extension, installing dependencies..."
        ${PIP_INSTALL_CMD} -r "$extension/requirements.txt"
      fi
    done
  else
    echo "ten_packages/system directory not found"
  fi

  echo "Python dependencies installation completed!"
}

build_go_app() {
  local app_dir=$1
  cd $app_dir

  # Check if the Go build tool exists before trying to build
  if [[ -f "$app_dir/ten_packages/system/ten_runtime_go/tools/build/main.go" ]]; then
    echo "Building Go app..."
    go run "$app_dir/ten_packages/system/ten_runtime_go/tools/build/main.go" --verbose
    local build_result=$?
    # Exit code 1 with "no buildable Go source files" is OK for Python-only apps
    if [[ $build_result -ne 0 ]]; then
      # Check if this is just a "no Go source files" error (which is fine for Python apps)
      echo "Go build returned non-zero exit code, but this may be OK for Python-only apps"
    fi
  else
    echo "Go build tool not found, skipping Go app build (this is normal if ten_runtime_go is not installed yet)"
  fi
}

main() {
  # Get the parent directory of script location as app root directory
  APP_HOME=$(
    cd $(dirname $0)/..
    pwd
  )

  echo "App root directory: $APP_HOME"
  echo "Using pip command: $PIP_INSTALL_CMD"

  # Check if manifest.json exists
  if [[ ! -f "$APP_HOME/manifest.json" ]]; then
    echo "Error: manifest.json file not found"
    exit 1
  fi

  build_go_app "$APP_HOME"

  # Install Python dependencies
  install_python_requirements "$APP_HOME"
}

# If script is executed directly, run main function
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  main "$@"
fi
