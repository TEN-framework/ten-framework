#!/usr/bin/env bash

# Function to show usage
show_usage() {
    echo "Usage: $0 TENAPP_PATH"
    echo ""
    echo "Arguments:"
    echo "  TENAPP_PATH    Path to the tenapp folder (required)"
    echo ""
    echo "Examples:"
    echo "  $0 /path/to/tenapp                   # Use specified tenapp directory"
    echo "  $0 ../other-tenapp                   # Use relative path"
    echo "  $0 .                                 # Use current directory"
}

# Parse command line arguments
if [[ $# -eq 0 ]]; then
    echo "Error: TENAPP_PATH argument is required"
    show_usage
    exit 1
fi

if [[ $# -gt 1 ]]; then
    echo "Error: Too many arguments"
    show_usage
    exit 1
fi

if [[ "$1" == "-h" || "$1" == "--help" ]]; then
    show_usage
    exit 0
fi

# Use provided path
if [[ -d "$1" ]]; then
    APP_HOME=$(cd "$1" && pwd)
else
    echo "Error: Directory '$1' does not exist"
    exit 1
fi

echo "Using tenapp directory: $APP_HOME"
cd $APP_HOME

rm -rf .release
mkdir .release

# Function to check if file should be excluded based on .tenignore
should_exclude_file() {
    local file_path="$1"
    local tenignore_file="$2"

    if [[ ! -f "$tenignore_file" ]]; then
        return 1  # No .tenignore file, don't exclude
    fi

    # Read .tenignore patterns and check if file matches any
    while IFS= read -r pattern; do
        # Skip empty lines and comments
        [[ -z "$pattern" || "$pattern" =~ ^[[:space:]]*# ]] && continue

        # Remove leading/trailing whitespace
        pattern=$(echo "$pattern" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
        [[ -z "$pattern" ]] && continue

        # Simple pattern matching
        # Handle directory patterns (ending with /)
        if [[ "$pattern" == */ ]]; then
            # Remove trailing slash and check if path starts with pattern
            local dir_pattern="${pattern%/}"
            if [[ "$file_path" == "$dir_pattern"/* ]]; then
                return 0  # File should be excluded
            fi
        # Handle wildcard patterns like *.pyc
        elif [[ "$pattern" == *"*"* ]]; then
            # Convert * to shell pattern
            local shell_pattern="${pattern//\*/.*}"
            if [[ "$file_path" =~ $shell_pattern ]]; then
                return 0  # File should be excluded
            fi
        # Handle exact matches
        else
            if [[ "$file_path" == "$pattern" ]]; then
                return 0  # File should be excluded
            fi
        fi
    done < "$tenignore_file"

    return 1  # File should not be excluded
}


copy_package() {
    local package_type=$1
    local package_name=$2
    local source_dir="ten_packages/${package_type}/${package_name}"
    local target_dir=".release/ten_packages/${package_type}/${package_name}"
    local tenignore_file="$APP_HOME/.tenignore"

    mkdir -p "$target_dir"

    if [[ -d "$source_dir" ]]; then
        if [[ -f "$tenignore_file" ]]; then
            echo "Copying $package_name with .tenignore rules..."
            # Copy files while respecting .tenignore
            # Use a more reliable method for WSL environments
            cd "$source_dir"
            for file in $(ls -1 2>/dev/null); do
                if [[ -f "$file" ]]; then
                    # Check if file should be excluded
                    if should_exclude_file "$file" "$tenignore_file"; then
                        echo "  Excluding: $file"
                        continue
                    fi
                    echo "  Copying: $file"
                    cp "$file" "$APP_HOME/$target_dir/"
                elif [[ -d "$file" ]]; then
                    # Handle subdirectories
                    if should_exclude_file "$file" "$tenignore_file"; then
                        echo "  Excluding directory: $file"
                        continue
                    fi
                    echo "  Copying directory: $file"
                    mkdir -p "$APP_HOME/$target_dir/$file"
                    cp -r "$file"/* "$APP_HOME/$target_dir/$file/" 2>/dev/null || true
                fi
            done
            cd "$APP_HOME"
        else
            echo "Copying $package_name (no .tenignore found)..."
            cp -r "$source_dir"/* "$target_dir/" 2>/dev/null || true
        fi
    fi
}

cp -r bin .release
cp -r scripts .release
cp manifest.json .release
cp property.json .release

# copy packages
mkdir -p .release/ten_packages
for package_type in system extension_group extension addon_loader; do
    for package_path in ten_packages/${package_type}/*; do
        package_name=$(basename ${package_path})
        copy_package ${package_type} ${package_name}
    done
done
