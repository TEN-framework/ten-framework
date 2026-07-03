#!/bin/bash

# TEN Framework - TGN Installation Script
# Purpose: Install ten_gn build system on Linux/macOS
# Reference: https://github.com/TEN-framework/ten_gn

set -e

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
TGN_VERSION="${TGN_VERSION:-main}"
TGN_REPO="https://github.com/TEN-framework/ten_gn.git"
INSTALL_DIR="${TGN_INSTALL_DIR:-/usr/local/ten_gn}"

# Print colored messages
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1"
}

# Check if tgn is already installed
check_existing_tgn() {
    if [ -d "$INSTALL_DIR" ]; then
        echo ""
        print_warn "tgn is already installed at $INSTALL_DIR"

        # Check current version
        if [ -d "$INSTALL_DIR/.git" ]; then
            local current_version=$(cd "$INSTALL_DIR" && git describe --tags 2>/dev/null || git rev-parse --short HEAD 2>/dev/null || echo "unknown")
            print_info "Current version: $current_version"
        fi

        echo ""

        # In CI environment (non-interactive), automatically proceed
        if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
            print_info "CI environment detected, proceeding with reinstallation..."
            rm -rf "$INSTALL_DIR"
            return
        fi

        # Ask user if they want to continue
        read -p "Do you want to reinstall/upgrade tgn? [y/N]: " -n 1 -r
        echo ""

        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            print_info "Installation cancelled by user"
            echo ""
            print_info "💡 tgn is already available at: $INSTALL_DIR"
            print_info "   Add to PATH: export PATH=\"$INSTALL_DIR:\$PATH\""
            echo ""
            exit 0
        fi

        echo ""
        print_info "Proceeding with installation..."
        rm -rf "$INSTALL_DIR"
        echo ""
    fi
}

# Check prerequisites
check_prerequisites() {
    print_info "Checking prerequisites..."

    # Check for git
    if ! command -v git &> /dev/null; then
        print_error "git is not installed"
        print_info "Please install git first:"
        print_info "  Ubuntu/Debian: sudo apt-get install git"
        print_info "  macOS: brew install git"
        exit 1
    fi

    # Check for python3
    if ! command -v python3 &> /dev/null; then
        print_error "python3 is not installed"
        print_info "Please install Python 3.10+ first"
        exit 1
    fi

    local python_version=$(python3 --version 2>&1 | awk '{print $2}')
    print_info "✓ git: $(git --version | awk '{print $3}')"
    print_info "✓ python3: $python_version"
}

# Install tgn
install_tgn() {
    print_info "Installing tgn (TEN build system)..."
    print_info "Repository: $TGN_REPO"
    print_info "Version: $TGN_VERSION"
    print_info "Install directory: $INSTALL_DIR"
    echo ""

    # Create parent directory if it doesn't exist
    local parent_dir=$(dirname "$INSTALL_DIR")
    if [ ! -d "$parent_dir" ]; then
        print_info "Creating directory: $parent_dir"

        # Check if we need sudo
        if [ -w "$parent_dir" ] || mkdir -p "$parent_dir" 2>/dev/null; then
            # We have write permission
            :
        else
            print_warn "Need sudo permission to create $parent_dir"
            sudo mkdir -p "$parent_dir"
        fi
    fi

    # Clone the repository
    print_info "Cloning ten_gn repository..."

    # Check if we need sudo for installation
    if [ -w "$parent_dir" ]; then
        git clone "$TGN_REPO" "$INSTALL_DIR"
        cd "$INSTALL_DIR"
        git checkout "$TGN_VERSION"
    else
        print_warn "Need sudo permission to install to $INSTALL_DIR"
        sudo git clone "$TGN_REPO" "$INSTALL_DIR"
        cd "$INSTALL_DIR"
        sudo git checkout "$TGN_VERSION"
        # Make sure tgn script is executable
        sudo chmod +x "$INSTALL_DIR/tgn"
    fi

    # Ensure tgn script is executable
    chmod +x "$INSTALL_DIR/tgn" 2>/dev/null || sudo chmod +x "$INSTALL_DIR/tgn"

    echo ""
    print_info "✓ tgn installed successfully!"
}

# Configure PATH
configure_path() {
    print_info "Configuring PATH..."

    local shell_config=""
    if [ -n "$BASH_VERSION" ]; then
        shell_config="$HOME/.bashrc"
    elif [ -n "$ZSH_VERSION" ]; then
        shell_config="$HOME/.zshrc"
    else
        # Try to detect shell from SHELL environment variable
        case "$SHELL" in
            */bash)
                shell_config="$HOME/.bashrc"
                ;;
            */zsh)
                shell_config="$HOME/.zshrc"
                ;;
        esac
    fi

    # Check if PATH is already configured
    if [ -n "$shell_config" ] && [ -f "$shell_config" ]; then
        if grep -q "ten_gn" "$shell_config" 2>/dev/null; then
            print_info "✓ PATH already configured in $shell_config"
            return
        fi

        # In CI environment, skip modifying shell config
        if [ -n "$CI" ] || [ -n "$GITHUB_ACTIONS" ]; then
            print_info "CI environment detected, skipping shell configuration"
            return
        fi

        echo ""
        print_info "Add tgn to PATH by adding this line to $shell_config:"
        echo ""
        echo "    export PATH=\"$INSTALL_DIR:\$PATH\""
        echo ""

        read -p "Would you like to add it automatically? [Y/n]: " -n 1 -r
        echo ""

        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            echo "" >> "$shell_config"
            echo "# TEN build system (tgn)" >> "$shell_config"
            echo "export PATH=\"$INSTALL_DIR:\$PATH\"" >> "$shell_config"
            print_info "✓ Added to $shell_config"
            print_warn "Please run: source $shell_config"
        fi
    else
        echo ""
        print_info "To use tgn, add it to your PATH:"
        echo ""
        echo "    export PATH=\"$INSTALL_DIR:\$PATH\""
        echo ""
    fi
}

# Verify installation
verify_installation() {
    print_info "Verifying installation..."

    # Add to PATH for this session
    export PATH="$INSTALL_DIR:$PATH"

    if command -v tgn &> /dev/null; then
        print_info "✓ tgn is accessible in PATH"
        print_info "  Location: $(which tgn)"

        # Test tgn
        if "$INSTALL_DIR/tgn" --help > /dev/null 2>&1 || "$INSTALL_DIR/tgn" help > /dev/null 2>&1; then
            print_info "✓ tgn is working correctly"
        fi
    else
        print_warn "tgn is not in PATH yet"
        print_info "You can test it directly: $INSTALL_DIR/tgn"
    fi

    echo ""
    print_info "================================================"
    print_info "✅ tgn installation completed successfully!"
    print_info "================================================"
    echo ""
    print_info "Installation directory: $INSTALL_DIR"
    print_info "Version: $TGN_VERSION"
    echo ""
    print_info "💡 Quick start:"
    echo "  export PATH=\"$INSTALL_DIR:\$PATH\"  # Add to PATH"
    echo "  tgn gen linux x64 release           # Generate build files"
    echo "  tgn build linux x64 release         # Build project"
    echo ""
}

# Main installation flow
main() {
    echo ""
    echo "================================================"
    echo "TEN Framework - TGN Installation Script"
    echo "================================================"
    echo ""

    check_existing_tgn
    check_prerequisites
    install_tgn
    configure_path
    verify_installation
}

# Run main function
main
