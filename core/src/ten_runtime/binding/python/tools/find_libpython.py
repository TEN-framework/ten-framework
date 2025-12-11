#!/usr/bin/env python3
#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
"""
Find the path to libpython for the current Python interpreter.

This script uses Python's sysconfig module to locate the libpython library
file for the currently running Python interpreter. It respects the user's
Python environment (virtualenv, pyenv, conda, etc.).

The script outputs the path to stdout if found, or an empty string if not found.
"""

import sys
import os
import sysconfig


def find_libpython():
    """Find the libpython path for the current platform and Python version."""

    # Try to use sysconfig to get the library information directly
    libdir = sysconfig.get_config_var("LIBDIR")
    ldlibrary = sysconfig.get_config_var(
        "LDLIBRARY"
    )  # e.g., libpython3.10.so.1.0
    library = sysconfig.get_config_var("LIBRARY")  # e.g., libpython3.10.a
    instsoname = sysconfig.get_config_var(
        "INSTSONAME"
    )  # e.g., libpython3.10.so.1.0
    ldversion = sysconfig.get_config_var("LDVERSION")  # e.g., 3.10

    # Get version information
    major = sys.version_info.major
    minor = sys.version_info.minor
    version = f"{major}.{minor}"

    # Get multiarch for Debian/Ubuntu systems
    multiarch = sysconfig.get_config_var("MULTIARCH")

    # Get prefix paths
    prefix = sys.prefix
    base_prefix = getattr(sys, "base_prefix", prefix)

    # Build candidates list
    candidates = []

    # Platform-specific handling
    if sys.platform == "win32":
        # Windows: pythonXY.dll
        dll_name = f"python{major}{minor}.dll"
        candidates.extend(
            [
                os.path.join(prefix, dll_name),
                os.path.join(base_prefix, dll_name),
                os.path.join(sys.exec_prefix, dll_name),
            ]
        )

    elif sys.platform == "darwin":
        # macOS: Try sysconfig first, then framework paths
        if libdir and ldlibrary:
            # Try the library name from sysconfig
            candidates.append(os.path.join(libdir, ldlibrary))

        # Framework and dylib paths
        candidates.extend(
            [
                os.path.join(base_prefix, "Python"),  # Framework
                os.path.join(base_prefix, "lib", f"libpython{version}.dylib"),
                os.path.join(prefix, "lib", f"libpython{version}.dylib"),
            ]
        )

        # Standard framework locations
        candidates.extend(
            [
                f"/Library/Frameworks/Python.framework/Versions/{version}/Python",
                f"/usr/local/opt/python@{version}/Frameworks/Python.framework/Versions/{version}/Python",
                f"/opt/homebrew/opt/python@{version}/Frameworks/Python.framework/Versions/{version}/Python",
            ]
        )

    else:
        # Linux and other Unix-like systems
        # Use sysconfig information first
        if libdir:
            if ldlibrary:
                # Use LDLIBRARY (shared library name from config)
                candidates.append(os.path.join(libdir, ldlibrary))

                # Also try without version suffix (e.g., libpython3.10.so instead of libpython3.10.so.1.0)
                base_so = ldlibrary.split(".so")[0] + ".so"
                if base_so != ldlibrary:
                    candidates.append(os.path.join(libdir, base_so))

            if instsoname:
                # Use INSTSONAME if available
                candidates.append(os.path.join(libdir, instsoname))

            # Build standard names using LDVERSION and VERSION
            if ldversion:
                candidates.append(
                    os.path.join(libdir, f"libpython{ldversion}.so")
                )

            candidates.extend(
                [
                    os.path.join(libdir, f"libpython{version}.so"),
                    os.path.join(libdir, "libpython3.so"),
                ]
            )

            # Multiarch support (Debian/Ubuntu)
            if multiarch:
                if ldlibrary:
                    candidates.append(
                        os.path.join(libdir, multiarch, ldlibrary)
                    )
                    base_so = ldlibrary.split(".so")[0] + ".so"
                    if base_so != ldlibrary:
                        candidates.append(
                            os.path.join(libdir, multiarch, base_so)
                        )

                if ldversion:
                    candidates.append(
                        os.path.join(
                            libdir, multiarch, f"libpython{ldversion}.so"
                        )
                    )

                candidates.extend(
                    [
                        os.path.join(
                            libdir, multiarch, f"libpython{version}.so"
                        ),
                        os.path.join(libdir, multiarch, "libpython3.so"),
                    ]
                )

    # Find the first existing file
    for candidate in candidates:
        if candidate and os.path.exists(candidate):
            # Return the real path (resolve symlinks)
            return os.path.realpath(candidate)

    # Not found
    return ""


if __name__ == "__main__":
    result = find_libpython()
    print(result)
