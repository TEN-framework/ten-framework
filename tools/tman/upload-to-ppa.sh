#!/bin/bash

# Script for building and uploading packages to Ubuntu PPA
# Can be used in CI/CD environments or locally
# Requires environment variables for configuration

set -e

# ============ Read configuration from environment variables ============

MAINTAINER_NAME="${PPA_MAINTAINER_NAME}"
MAINTAINER_EMAIL="${PPA_MAINTAINER_EMAIL}"
GPG_KEY_ID="${PPA_GPG_KEY_ID}"
GPG_PASSPHRASE="${PPA_GPG_PASSPHRASE}"
LAUNCHPAD_ID="${PPA_LAUNCHPAD_ID}"
PPA_NAME="${PPA_PPA_NAME}"
PACKAGE_NAME="${PPA_PACKAGE_NAME:-tman}"
VERSION="${PPA_VERSION}"
REVISION="${PPA_REVISION:-1}"
DISTRIBUTIONS="${PPA_DISTRIBUTIONS:-jammy noble oracular plucky questing}"
SOURCE_BINARY="${PPA_SOURCE_BINARY}"
ARCH="${PPA_ARCH:-amd64}"

# ============ Validate required environment variables ============

if [ -z "$MAINTAINER_NAME" ] || [ -z "$MAINTAINER_EMAIL" ] || [ -z "$GPG_KEY_ID" ] || \
   [ -z "$LAUNCHPAD_ID" ] || [ -z "$PPA_NAME" ] || [ -z "$VERSION" ] || [ -z "$SOURCE_BINARY" ]; then
    echo "Error: Missing required environment variables"
    echo "Required environment variables:"
    echo "  PPA_MAINTAINER_NAME: ${MAINTAINER_NAME:-not set}"
    echo "  PPA_MAINTAINER_EMAIL: ${MAINTAINER_EMAIL:-not set}"
    echo "  PPA_GPG_KEY_ID: ${GPG_KEY_ID:-not set}"
    echo "  PPA_LAUNCHPAD_ID: ${LAUNCHPAD_ID:-not set}"
    echo "  PPA_PPA_NAME: ${PPA_NAME:-not set}"
    echo "  PPA_VERSION: ${VERSION:-not set}"
    echo "  PPA_SOURCE_BINARY: ${SOURCE_BINARY:-not set}"
    echo "  PPA_ARCH: ${ARCH:-not set}"
    exit 1
fi

# ============ Log functions ============

log_info() {
    echo "[INFO] $1"
}

log_error() {
    echo "[ERROR] $1" >&2
}

# ============ Start build process ============

log_info "=========================================="
log_info "   PPA Build and Upload"
log_info "=========================================="
log_info "Package: $PACKAGE_NAME"
log_info "Version: $VERSION"
log_info "Architecture: $ARCH"
log_info "Distributions: $DISTRIBUTIONS"
log_info "PPA: ppa:${LAUNCHPAD_ID}/${PPA_NAME}"
log_info "=========================================="

# Check source file
if [ ! -f "$SOURCE_BINARY" ]; then
    log_error "Source file not found: $SOURCE_BINARY"
    exit 1
fi

log_info "Source file: $SOURCE_BINARY ($(du -h "$SOURCE_BINARY" | cut -f1))"

# Build and upload for each distribution
for dist in $DISTRIBUTIONS; do
    log_info "=========================================="
    log_info "Processing distribution: $dist"
    log_info "=========================================="

    # Use architecture-specific revision to avoid version conflicts
    # This allows uploading different architectures of the same version
    ARCH_REVISION="${REVISION}${ARCH}"

    WORK_DIR="/tmp/ppa-build-${PACKAGE_NAME}-${dist}"
    PACKAGE_DIR="${WORK_DIR}/${PACKAGE_NAME}-${VERSION}"

    log_info "Creating work directory: $WORK_DIR"
    rm -rf "$WORK_DIR"
    mkdir -p "$PACKAGE_DIR/bin"
    mkdir -p "$PACKAGE_DIR/debian/source"

    # Copy source files
    log_info "Copying source files..."
    cp "$SOURCE_BINARY" "$PACKAGE_DIR/bin/"
    chmod 755 "$PACKAGE_DIR/bin/$(basename "$SOURCE_BINARY")"

    # Create Makefile
    log_info "Creating Makefile..."
    cat > "$PACKAGE_DIR/Makefile" << 'EOF'
PREFIX ?= /usr
DESTDIR ?=

all:
	@echo "Nothing to compile, binary is pre-built"

install:
	install -D -m 0755 bin/tman $(DESTDIR)$(PREFIX)/bin/tman

clean:
	@echo "Nothing to clean"

.PHONY: all install clean
EOF

    # Create debian/changelog
    log_info "Creating debian/changelog..."
    cat > "$PACKAGE_DIR/debian/changelog" << EOF
${PACKAGE_NAME} (${VERSION}ubuntu${ARCH_REVISION}~${dist}) ${dist}; urgency=medium

  * Release version ${VERSION} for ${ARCH}
  * TEN Framework Package Manager

 -- ${MAINTAINER_NAME} <${MAINTAINER_EMAIL}>  $(date -R)
EOF

    # Create debian/control
    log_info "Creating debian/control..."
    cat > "$PACKAGE_DIR/debian/control" << EOF
Source: ${PACKAGE_NAME}
Section: utils
Priority: optional
Maintainer: ${MAINTAINER_NAME} <${MAINTAINER_EMAIL}>
Build-Depends: debhelper (>= 13)
Standards-Version: 4.6.0
Homepage: https://github.com/TEN-framework/ten-framework

Package: ${PACKAGE_NAME}
Architecture: ${ARCH}
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: TEN Framework Package Manager
 tman is a package management tool for the TEN framework.
 It helps developers manage extensions, protocols, and other
 TEN framework components.
 .
 Features:
  - Install and manage TEN packages
  - Dependency resolution
  - Package version control
  - Support for multiple package types
EOF

    # Create debian/rules
    log_info "Creating debian/rules..."
    cat > "$PACKAGE_DIR/debian/rules" << 'EOF'
#!/usr/bin/make -f

%:
	dh $@

override_dh_auto_test:
	@echo "Skipping tests for binary package"

override_dh_shlibdeps:
	dh_shlibdeps --dpkg-shlibdeps-params=--ignore-missing-info

override_dh_strip:
	dh_strip --no-automatic-dbgsym
EOF
    chmod +x "$PACKAGE_DIR/debian/rules"

    # Create other debian files
    log_info "Creating other debian files..."
    echo "13" > "$PACKAGE_DIR/debian/compat"
    echo "3.0 (native)" > "$PACKAGE_DIR/debian/source/format"

    cat > "$PACKAGE_DIR/debian/copyright" << EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: ${PACKAGE_NAME}
Upstream-Contact: ${MAINTAINER_NAME} <${MAINTAINER_EMAIL}>
Source: https://github.com/TEN-framework/ten-framework

Files: *
Copyright: 2025 Agora
License: Apache-2.0-with-TEN-Additional-Conditions

License: Apache-2.0-with-TEN-Additional-Conditions
 The TEN Framework is licensed pursuant to the Apache License v2.0
 with additional conditions.
 .
 For the complete license text, please see:
 https://github.com/TEN-framework/ten-framework/blob/main/LICENSE
EOF

    cat > "$PACKAGE_DIR/debian/install" << EOF
bin/tman usr/bin
EOF

    # Build source package
    log_info "Building source package for $dist..."
    cd "$PACKAGE_DIR"

    # Run debuild
    log_info "Running debuild..."

    # Configure for non-interactive signing in CI environments
    if [ -n "$GPG_PASSPHRASE" ]; then
        # Create a passphrase file
        PASSPHRASE_FILE=$(mktemp)
        echo "$GPG_PASSPHRASE" > "$PASSPHRASE_FILE"
        chmod 600 "$PASSPHRASE_FILE"

        # Build unsigned package first (skip lintian and signing)
        export LINTIAN=:
        cd "$PACKAGE_DIR"
        dpkg-buildpackage -S -sa -d -us -uc 2>&1 | tee "$WORK_DIR/debuild.log"
        BUILD_EXIT=${PIPESTATUS[0]}

        if [ $BUILD_EXIT -ne 0 ]; then
            log_error "Build failed! See log: $WORK_DIR/debuild.log"
            rm -f "$PASSPHRASE_FILE"
            exit 1
        fi

        # Sign the files manually using debsign
        cd "$WORK_DIR"
        log_info "Signing packages..."

        changes_file="${PACKAGE_NAME}_${VERSION}ubuntu${ARCH_REVISION}~${dist}_source.changes"
        debsign --no-re-sign -k"$GPG_KEY_ID" \
                -p"gpg --batch --passphrase-file $PASSPHRASE_FILE --pinentry-mode loopback" \
                "$changes_file" 2>&1 | tee -a "$WORK_DIR/debuild.log"
        SIGN_EXIT=${PIPESTATUS[0]}

        # Clean up passphrase file
        rm -f "$PASSPHRASE_FILE"
        unset LINTIAN

        if [ $SIGN_EXIT -ne 0 ]; then
            log_error "Signing failed! See log: $WORK_DIR/debuild.log"
            exit 1
        fi
    else
        # Interactive signing for local use
        export LINTIAN=:
        debuild -S -sa -d -k"$GPG_KEY_ID" 2>&1 | tee "$WORK_DIR/debuild.log"
        DEBUILD_EXIT=${PIPESTATUS[0]}
        unset LINTIAN

        if [ $DEBUILD_EXIT -ne 0 ]; then
            log_error "Build failed! See log: $WORK_DIR/debuild.log"
            exit 1
        fi
    fi

    log_info "Source package built successfully ✓"

    # Upload to PPA
    log_info "Uploading to PPA: ppa:${LAUNCHPAD_ID}/${PPA_NAME} for $dist..."
    cd "$WORK_DIR"

    changes_file="${PACKAGE_NAME}_${VERSION}ubuntu${ARCH_REVISION}~${dist}_source.changes"

    if [ ! -f "$changes_file" ]; then
        log_error "Changes file not found: $changes_file"
        exit 1
    fi

    log_info "Uploading file: $changes_file"

    # Try using dput first
    if dput "ppa:${LAUNCHPAD_ID}/${PPA_NAME}" "$changes_file" 2>&1; then
        log_info "Upload successful ✓"
    else
        log_error "Upload failed"
        exit 1
    fi
done

log_info "=========================================="
log_info "   Complete!"
log_info "=========================================="
log_info "View your PPA:"
log_info "https://launchpad.net/~${LAUNCHPAD_ID}/+archive/ubuntu/${PPA_NAME}"
