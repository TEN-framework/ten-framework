#!/usr/bin/env python3
"""
TEN Framework Dependencies Installation Script

This script will:
1. Install Python dependencies for all extensions and system packages
2. Install Node.js dependencies for all extensions and system packages

Usage:
    python3 scripts/install_deps.py
    python3 scripts/install_deps.py --pip-index https://pypi.org/simple
    python3 scripts/install_deps.py --npm-registry https://registry.npmjs.org
    python3 scripts/install_deps.py --python-version 3.10
"""

import argparse
import subprocess
import sys
from pathlib import Path
from typing import List, Tuple


class Colors:
    """ANSI color codes for terminal output"""

    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"


def print_header(message: str):
    """Print a formatted header message"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}{'=' * 70}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{message}{Colors.ENDC}")
    print(f"{Colors.HEADER}{Colors.BOLD}{'=' * 70}{Colors.ENDC}\n")


def print_success(message: str):
    """Print a success message"""
    print(f"{Colors.OKGREEN}✓ {message}{Colors.ENDC}")


def print_error(message: str):
    """Print an error message"""
    print(f"{Colors.FAIL}✗ {message}{Colors.ENDC}")


def print_info(message: str):
    """Print an info message"""
    print(f"{Colors.OKCYAN}→ {message}{Colors.ENDC}")


def find_python_executable(version: str | None = None) -> str:
    """
    Find and return the Python executable path.

    Args:
        version: Specific version like "3.10" or None for any python3

    Returns:
        Full path to Python executable

    Raises:
        RuntimeError: If Python is not found
    """
    if version:
        python_names = [f"python{version}", f'python{version.replace(".", "")}']
    else:
        python_names = ["python3", "python"]

    for name in python_names:
        try:
            # Check if the command exists
            result = subprocess.run(
                [name, "--version"], capture_output=True, text=True, check=True
            )
            version_output = result.stdout.strip()

            # If specific version is requested, verify it
            if version and version not in version_output:
                continue

            # Get full path
            if sys.platform == "win32":
                full_path = (
                    subprocess.run(
                        ["where", name],
                        capture_output=True,
                        text=True,
                        check=True,
                    )
                    .stdout.strip()
                    .split("\n")[0]
                )
            else:
                full_path = subprocess.run(
                    ["which", name], capture_output=True, text=True, check=True
                ).stdout.strip()

            return full_path
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue

    version_str = f"Python {version}" if version else "Python 3"
    raise RuntimeError(
        f"{version_str} not found. Please install {version_str} and ensure "
        f"it is available in your PATH."
    )


def find_npm_executable() -> str:
    """
    Find and return the npm executable path.

    Returns:
        Full path to npm executable

    Raises:
        RuntimeError: If npm is not found
    """
    try:
        result = subprocess.run(
            ["npm", "--version"], capture_output=True, text=True, check=True
        )

        if sys.platform == "win32":
            full_path = (
                subprocess.run(
                    ["where", "npm"], capture_output=True, text=True, check=True
                )
                .stdout.strip()
                .split("\n")[0]
            )
        else:
            full_path = subprocess.run(
                ["which", "npm"], capture_output=True, text=True, check=True
            ).stdout.strip()

        return full_path
    except (subprocess.CalledProcessError, FileNotFoundError):
        raise RuntimeError(
            "npm not found. Please install Node.js and npm and ensure "
            "they are available in your PATH."
        )


def find_requirements_files(root_dir: Path) -> List[Tuple[Path, str]]:
    """
    Find all requirements.txt files in ten_packages directory.

    Args:
        root_dir: Root directory of the project

    Returns:
        List of tuples (file_path, package_name)
    """
    requirements_files = []
    ten_packages_dir = root_dir / "ten_packages"

    if not ten_packages_dir.exists():
        return requirements_files

    # Search in extension and system directories
    for subdir in ["extension", "system"]:
        search_dir = ten_packages_dir / subdir
        if not search_dir.exists():
            continue

        for package_dir in search_dir.iterdir():
            if not package_dir.is_dir():
                continue

            requirements_file = package_dir / "requirements.txt"
            if requirements_file.exists():
                package_name = f"{subdir}/{package_dir.name}"
                requirements_files.append((requirements_file, package_name))

    return requirements_files


def find_package_json_files(root_dir: Path) -> List[Tuple[Path, str]]:
    """
    Find all package.json files in ten_packages directory.

    Args:
        root_dir: Root directory of the project

    Returns:
        List of tuples (directory_path, package_name)
    """
    package_json_dirs = []
    ten_packages_dir = root_dir / "ten_packages"

    if not ten_packages_dir.exists():
        return package_json_dirs

    # Search in extension and system directories
    for subdir in ["extension", "system"]:
        search_dir = ten_packages_dir / subdir
        if not search_dir.exists():
            continue

        for package_dir in search_dir.iterdir():
            if not package_dir.is_dir():
                continue

            package_json = package_dir / "package.json"
            if package_json.exists():
                package_name = f"{subdir}/{package_dir.name}"
                package_json_dirs.append((package_dir, package_name))

    return package_json_dirs


def install_python_requirements(
    python_path: str,
    requirements_file: Path,
    package_name: str,
    pip_index: str | None = None,
) -> bool:
    """
    Install Python requirements from a requirements.txt file.

    Args:
        python_path: Path to Python executable
        requirements_file: Path to requirements.txt
        package_name: Name of the package (for display)
        pip_index: Optional pip index URL

    Returns:
        True if successful, False otherwise
    """
    print_info(f"Installing Python dependencies for {package_name}...")

    pip_args = [
        python_path,
        "-m",
        "pip",
        "install",
        "-r",
        str(requirements_file),
    ]

    if pip_index:
        pip_args.extend(["-i", pip_index])

    try:
        subprocess.run(
            pip_args,
            check=True,
            capture_output=False,
        )
        print_success(f"Installed dependencies for {package_name}")
        return True
    except subprocess.CalledProcessError as e:
        print_error(
            f"Failed to install dependencies for {package_name} (exit code: {e.returncode})"
        )
        return False


def install_nodejs_dependencies(
    npm_path: str,
    package_dir: Path,
    package_name: str,
    npm_registry: str | None = None,
) -> bool:
    """
    Install Node.js dependencies from a package.json file.

    Args:
        npm_path: Path to npm executable
        package_dir: Directory containing package.json
        package_name: Name of the package (for display)
        npm_registry: Optional npm registry URL

    Returns:
        True if successful, False otherwise
    """
    print_info(f"Installing Node.js dependencies for {package_name}...")

    npm_args = [npm_path, "install"]

    if npm_registry:
        npm_args.extend(["--registry", npm_registry])

    try:
        subprocess.run(
            npm_args,
            cwd=str(package_dir),
            check=True,
            capture_output=False,
        )
        print_success(f"Installed dependencies for {package_name}")
        return True
    except subprocess.CalledProcessError as e:
        print_error(
            f"Failed to install dependencies for {package_name} (exit code: {e.returncode})"
        )
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Install Python and Node.js dependencies for TEN Framework project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--python-version",
        type=str,
        required=False,
        default=None,
        help="Specific Python version to use (e.g., 3.10). If not specified, uses default python3.",
    )
    parser.add_argument(
        "--pip-index",
        type=str,
        required=False,
        default=None,
        help="Specify pip index URL (e.g., https://pypi.org/simple)",
    )
    parser.add_argument(
        "--npm-registry",
        type=str,
        required=False,
        default=None,
        help="Specify npm registry URL (e.g., https://registry.npmjs.org)",
    )
    parser.add_argument(
        "--skip-python",
        action="store_true",
        help="Skip Python dependencies installation",
    )
    parser.add_argument(
        "--skip-nodejs",
        action="store_true",
        help="Skip Node.js dependencies installation",
    )

    args = parser.parse_args()

    # Get project root directory
    script_dir = Path(__file__).parent
    root_dir = script_dir.parent.resolve()

    print_header("TEN Framework Dependencies Installation")
    print_info(f"Project root: {root_dir}")

    # Check if manifest.json exists
    manifest_file = root_dir / "manifest.json"
    if not manifest_file.exists():
        print_error("manifest.json not found in project root")
        return 1

    success = True

    # Install Python dependencies
    if not args.skip_python:
        print_header("Installing Python Dependencies")

        try:
            python_path = find_python_executable(args.python_version)
            print_success(f"Found Python: {python_path}")

            # Get Python version
            version_result = subprocess.run(
                [python_path, "--version"],
                capture_output=True,
                text=True,
                check=True,
            )
            print_info(f"Version: {version_result.stdout.strip()}")

        except RuntimeError as e:
            print_error(str(e))
            return 1

        if args.pip_index:
            print_info(f"Using pip index: {args.pip_index}")

        requirements_files = find_requirements_files(root_dir)

        if not requirements_files:
            print_info("No Python packages with requirements.txt found")
        else:
            print_info(
                f"Found {len(requirements_files)} Python package(s) with requirements.txt"
            )
            print()

            for req_file, package_name in requirements_files:
                if not install_python_requirements(
                    python_path, req_file, package_name, args.pip_index
                ):
                    success = False

    # Install Node.js dependencies
    if not args.skip_nodejs:
        print_header("Installing Node.js Dependencies")

        try:
            npm_path = find_npm_executable()
            print_success(f"Found npm: {npm_path}")

            # Get npm version
            version_result = subprocess.run(
                [npm_path, "--version"],
                capture_output=True,
                text=True,
                check=True,
            )
            print_info(f"Version: {version_result.stdout.strip()}")

        except RuntimeError as e:
            print_error(str(e))
            return 1

        if args.npm_registry:
            print_info(f"Using npm registry: {args.npm_registry}")

        package_json_dirs = find_package_json_files(root_dir)

        if not package_json_dirs:
            print_info("No Node.js packages with package.json found")
        else:
            print_info(
                f"Found {len(package_json_dirs)} Node.js package(s) with package.json"
            )
            print()

            for package_dir, package_name in package_json_dirs:
                if not install_nodejs_dependencies(
                    npm_path, package_dir, package_name, args.npm_registry
                ):
                    success = False

    # Final summary
    print_header("Installation Summary")

    if success:
        print_success("All dependencies installed successfully!")
        return 0
    else:
        print_error(
            "Some dependencies failed to install. Please check the logs above."
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
