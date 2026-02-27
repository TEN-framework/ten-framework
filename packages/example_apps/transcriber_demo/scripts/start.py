#!/usr/bin/env python3
"""Cross-platform start script for TEN Framework transcriber demo."""

import os
import subprocess
import sys
from pathlib import Path


def _load_env_file(env_path: Path) -> None:
    """Load .env file into os.environ (simple key=value parser)."""
    if not env_path.exists():
        return
    with open(env_path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                continue
            key, _, value = line.partition("=")
            key = key.strip()
            value = value.strip()
            # Don't override existing env vars
            if key and key not in os.environ:
                os.environ[key] = value


def main():
    # Resolve project root (scripts/ -> parent)
    root_dir = Path(__file__).resolve().parent.parent
    os.chdir(root_dir)

    # Load .env file so ${env:...} placeholders in property.json can resolve
    _load_env_file(root_dir / ".env")

    is_win = sys.platform == "win32"
    sep = ";" if is_win else ":"

    # Find libpython and set TEN_PYTHON_LIB_PATH
    find_libpython = (
        root_dir
        / "ten_packages"
        / "system"
        / "ten_runtime_python"
        / "tools"
        / "find_libpython.py"
    )
    if find_libpython.exists():
        try:
            result = subprocess.run(
                [sys.executable, str(find_libpython)],
                capture_output=True,
                text=True,
            )
            if result.returncode == 0 and result.stdout.strip():
                os.environ["TEN_PYTHON_LIB_PATH"] = result.stdout.strip()
        except Exception:
            pass

    # Set PYTHONPATH
    cwd = str(root_dir)
    ai_base_interface = str(
        root_dir / "ten_packages" / "system" / "ten_ai_base" / "interface"
    )
    existing = os.environ.get("PYTHONPATH", "")
    os.environ["PYTHONPATH"] = sep.join(
        filter(None, [cwd, ai_base_interface, existing])
    )

    # Set NODE_PATH
    nodejs_lib = str(
        root_dir / "ten_packages" / "system" / "ten_runtime_nodejs" / "lib"
    )
    existing_node = os.environ.get("NODE_PATH", "")
    os.environ["NODE_PATH"] = sep.join(
        filter(None, [nodejs_lib, existing_node])
    )

    # On Windows, add runtime DLL directories to PATH
    if is_win:
        dll_dirs = [
            str(root_dir / "ten_packages" / "system" / "ten_runtime_go" / "lib"),
            str(root_dir / "ten_packages" / "system" / "ten_runtime" / "lib"),
        ]
        os.environ["PATH"] = sep.join(
            dll_dirs + [os.environ.get("PATH", "")]
        )

    # Determine executable
    main_exe = root_dir / "bin" / ("main.exe" if is_win else "main")

    # Build command with any extra args (e.g. --property property-vad.json)
    cmd = [str(main_exe)] + sys.argv[1:]

    # Replace current process (exec on Unix, subprocess on Windows)
    if is_win:
        result = subprocess.run(cmd)
        sys.exit(result.returncode)
    else:
        os.execv(str(main_exe), cmd)


if __name__ == "__main__":
    main()
