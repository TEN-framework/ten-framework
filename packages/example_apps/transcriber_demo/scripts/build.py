#!/usr/bin/env python3
"""
TEN Framework Build Script

This script will:
1. Build the main Go application
2. Build all Go extensions
3. Build all Node.js extensions

Usage:
    python3 scripts/build.py
    python3 scripts/build.py --skip-go
    python3 scripts/build.py --skip-nodejs
    python3 scripts/build.py --npm-cmd npm
"""

import argparse
import os
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


def print_warning(message: str):
    """Print a warning message"""
    print(f"{Colors.WARNING}⚠ {message}{Colors.ENDC}")


def find_go_executable() -> str:
    """
    Find and return the go executable path.

    Returns:
        Full path to go executable

    Raises:
        RuntimeError: If go is not found
    """
    try:
        result = subprocess.run(
            ["go", "version"], capture_output=True, text=True, check=True
        )

        if sys.platform == "win32":
            full_path = (
                subprocess.run(
                    ["where", "go"], capture_output=True, text=True, check=True
                )
                .stdout.strip()
                .split("\n")[0]
            )
        else:
            full_path = subprocess.run(
                ["which", "go"], capture_output=True, text=True, check=True
            ).stdout.strip()

        return full_path
    except (subprocess.CalledProcessError, FileNotFoundError):
        raise RuntimeError(
            "go not found. Please install Go and ensure it is available in your PATH."
        )


def find_npm_executable(npm_cmd: str = "npm") -> str:
    """
    Find and return the npm executable path.

    Args:
        npm_cmd: npm command to use (default: "npm")

    Returns:
        Full path to npm executable

    Raises:
        RuntimeError: If npm is not found
    """
    try:
        result = subprocess.run(
            [npm_cmd, "--version"], capture_output=True, text=True, check=True
        )

        if sys.platform == "win32":
            full_path = (
                subprocess.run(
                    ["where", npm_cmd],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                .stdout.strip()
                .split("\n")[0]
            )
        else:
            full_path = subprocess.run(
                ["which", npm_cmd], capture_output=True, text=True, check=True
            ).stdout.strip()

        return full_path
    except (subprocess.CalledProcessError, FileNotFoundError):
        raise RuntimeError(
            f"{npm_cmd} not found. Please install Node.js and npm and ensure "
            "they are available in your PATH."
        )


def find_go_projects(root_dir: Path) -> List[Tuple[Path, str]]:
    """
    Find all Go projects (with go.mod files).

    Note: Only the main app is returned because Go extensions are built automatically
    by the TEN Framework build tool when building the main app.

    Args:
        root_dir: Root directory of the project

    Returns:
        List of tuples (project_dir, project_name)
    """
    go_projects = []

    # Check main app
    main_go_mod = root_dir / "go.mod"
    if main_go_mod.exists():
        go_projects.append((root_dir, "main app"))

    return go_projects


def find_nodejs_projects(root_dir: Path) -> List[Tuple[Path, str]]:
    """
    Find all Node.js projects (with package.json files) that have build scripts.

    Args:
        root_dir: Root directory of the project

    Returns:
        List of tuples (project_dir, project_name)
    """
    nodejs_projects = []

    # Check main app
    main_package_json = root_dir / "package.json"
    if main_package_json.exists():
        nodejs_projects.append((root_dir, "main app"))

    # Check Node.js extensions
    ten_packages_dir = root_dir / "ten_packages"
    if ten_packages_dir.exists():
        extension_dir = ten_packages_dir / "extension"
        if extension_dir.exists():
            for package_dir in extension_dir.iterdir():
                if not package_dir.is_dir():
                    continue

                package_json = package_dir / "package.json"
                if package_json.exists():
                    package_name = f"extension/{package_dir.name}"
                    nodejs_projects.append((package_dir, package_name))

    return nodejs_projects


def build_go_project(
    go_path: str,
    project_dir: Path,
    project_name: str,
) -> bool:
    """
    Build a Go project using TEN Framework's build tool.

    Args:
        go_path: Path to go executable
        project_dir: Directory containing go.mod
        project_name: Name of the project (for display)

    Returns:
        True if successful, False otherwise
    """
    print_info(f"Building {project_name}...")

    # Use TEN Framework's build tool for the main app
    if project_name == "main app":
        build_tool = (
            project_dir
            / "ten_packages"
            / "system"
            / "ten_runtime_go"
            / "tools"
            / "build"
            / "main.go"
        )

        if not build_tool.exists():
            print_error(f"TEN Framework build tool not found at {build_tool}")
            return False

        build_args = [go_path, "run", str(build_tool), "--verbose"]

        try:
            subprocess.run(
                build_args,
                cwd=str(project_dir),
                check=True,
                capture_output=False,
            )
            print_success(f"Built {project_name}")
            return True
        except subprocess.CalledProcessError as e:
            print_error(
                f"Failed to build {project_name} (exit code: {e.returncode})"
            )
            return False
    else:
        # For Go extensions, we don't need to build them separately
        # They are built together with the main app by the TEN Framework build tool
        print_info(f"Skipping {project_name} (will be built with main app)")
        return True


def has_build_script(npm_path: str, project_dir: Path) -> bool:
    """
    Check if a Node.js project has a build script.

    Args:
        npm_path: Path to npm executable
        project_dir: Directory containing package.json

    Returns:
        True if build script exists, False otherwise
    """
    try:
        result = subprocess.run(
            [npm_path, "run", "build", "--dry-run"],
            cwd=str(project_dir),
            capture_output=True,
            text=True,
            check=False,
        )
        # If dry-run succeeds, build script exists
        return result.returncode == 0
    except Exception:
        return False


def build_nodejs_project(
    npm_path: str,
    project_dir: Path,
    project_name: str,
) -> bool:
    """
    Build a Node.js project.

    Args:
        npm_path: Path to npm executable
        project_dir: Directory containing package.json
        project_name: Name of the project (for display)

    Returns:
        True if successful, False otherwise
    """
    print_info(f"Building {project_name}...")

    # Check if build script exists
    if not has_build_script(npm_path, project_dir):
        print_warning(
            f"No build script found for {project_name}, skipping build step"
        )
        return True

    # Clean build directory and TypeScript build info before building
    # This ensures a fresh build every time
    build_dir = project_dir / "build"
    tsbuildinfo = project_dir / "tsconfig.tsbuildinfo"

    try:
        if build_dir.exists():
            import shutil

            shutil.rmtree(build_dir)
            print_info(f"Cleaned {project_name} build directory")

        if tsbuildinfo.exists():
            tsbuildinfo.unlink()
            print_info(f"Cleaned {project_name} TypeScript build cache")
    except Exception as e:
        print_warning(
            f"Failed to clean build artifacts for {project_name}: {e}"
        )

    try:
        subprocess.run(
            [npm_path, "run", "build"],
            cwd=str(project_dir),
            check=True,
            capture_output=False,
        )
        print_success(f"Built {project_name}")
        return True
    except subprocess.CalledProcessError as e:
        print_error(
            f"Failed to build {project_name} (exit code: {e.returncode})"
        )
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Build Go and Node.js projects for TEN Framework",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--npm-cmd",
        type=str,
        default="npm",
        help="npm command to use (default: npm)",
    )
    parser.add_argument(
        "--skip-go",
        action="store_true",
        help="Skip Go projects build",
    )
    parser.add_argument(
        "--skip-nodejs",
        action="store_true",
        help="Skip Node.js projects build",
    )

    args = parser.parse_args()

    # Get project root directory
    script_dir = Path(__file__).parent
    root_dir = script_dir.parent.resolve()

    print_header("TEN Framework Build")
    print_info(f"Project root: {root_dir}")

    # Check if manifest.json exists
    manifest_file = root_dir / "manifest.json"
    if not manifest_file.exists():
        print_error("manifest.json not found in project root")
        return 1

    success = True

    # Build Go projects
    if not args.skip_go:
        print_header("Building Go Projects")

        try:
            go_path = find_go_executable()
            print_success(f"Found Go: {go_path}")

            # Get Go version
            version_result = subprocess.run(
                [go_path, "version"], capture_output=True, text=True, check=True
            )
            print_info(f"Version: {version_result.stdout.strip()}")

        except RuntimeError as e:
            print_error(str(e))
            return 1

        go_projects = find_go_projects(root_dir)

        if not go_projects:
            print_info("No Go projects found")
        else:
            print_info(f"Found {len(go_projects)} Go project(s)")
            print()

            for project_dir, project_name in go_projects:
                if not build_go_project(go_path, project_dir, project_name):
                    success = False

    # Build Node.js projects
    if not args.skip_nodejs:
        print_header("Building Node.js Projects")

        try:
            npm_path = find_npm_executable(args.npm_cmd)
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

        nodejs_projects = find_nodejs_projects(root_dir)

        if not nodejs_projects:
            print_info("No Node.js projects found")
        else:
            print_info(f"Found {len(nodejs_projects)} Node.js project(s)")
            print()

            for project_dir, project_name in nodejs_projects:
                if not build_nodejs_project(
                    npm_path, project_dir, project_name
                ):
                    success = False

    # Final summary
    print_header("Build Summary")

    if success:
        print_success("All projects built successfully!")
        return 0
    else:
        print_error(
            "Some projects failed to build. Please check the logs above."
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
