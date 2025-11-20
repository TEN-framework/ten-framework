#!/usr/bin/env python3
"""
TEN Framework Build Script

This script will:
1. Build the main Go application
2. Build all Go extensions
3. Build all Node.js extensions
4. Build all C++ extensions

Usage:
    # Auto-detect OS and architecture (recommended)
    python3 scripts/build.py

    # Skip specific build steps
    python3 scripts/build.py --skip-go
    python3 scripts/build.py --skip-nodejs
    python3 scripts/build.py --skip-cxx

    # Use custom npm command
    python3 scripts/build.py --npm-cmd npm

    # Manually specify OS and architecture
    python3 scripts/build.py --os linux --cpu x64 --build-type release
    python3 scripts/build.py --os mac --cpu arm64 --build-type debug
"""

import argparse
import platform
import shutil
import subprocess
import sys
from pathlib import Path
from typing import List, Tuple


def detect_os() -> str:
    """Detect operating system in the format needed for tgn."""
    system = platform.system().lower()

    if system == "linux":
        return "linux"

    if system == "darwin":
        return "mac"

    if system == "windows":
        return "win"

    raise RuntimeError(f"Unsupported OS: {system}")


def detect_arch() -> str:
    """Detect architecture in the format needed for tgn."""
    machine = platform.machine().lower()

    if machine in ("x86_64", "amd64"):
        return "x64"

    if machine in ("i386", "i686", "x86"):
        return "x86"

    if machine in ("arm64", "aarch64"):
        return "arm64"

    if machine.startswith("arm"):
        return "arm"

    raise RuntimeError(f"Unsupported architecture: {machine}")


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
        subprocess.run(
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
        error_msg = (
            "go not found. Please install Go and ensure it is available "
            "in your PATH."
        )
        raise RuntimeError(error_msg)


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
        subprocess.run(
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
        error_msg = (
            f"{npm_cmd} not found. Please install Node.js and npm and ensure "
            "they are available in your PATH."
        )
        raise RuntimeError(error_msg)


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


def check_build_gn(root_dir: Path) -> bool:
    """
    Check if scripts/BUILD.gn exists for building C++ extensions.

    Args:
        root_dir: Root directory of the project

    Returns:
        True if BUILD.gn exists, False otherwise
    """
    build_gn = root_dir / "scripts" / "BUILD.gn"
    return build_gn.exists()


def find_tgn_executable() -> str:
    """
    Find and return the tgn executable path.

    Returns:
        Full path to tgn executable

    Raises:
        RuntimeError: If tgn is not found
    """
    try:
        subprocess.run(
            ["tgn", "--version"], capture_output=True, text=True, check=True
        )

        if sys.platform == "win32":
            full_path = (
                subprocess.run(
                    ["where", "tgn"],
                    capture_output=True,
                    text=True,
                    check=True,
                )
                .stdout.strip()
                .split("\n")[0]
            )
        else:
            full_path = subprocess.run(
                ["which", "tgn"], capture_output=True, text=True, check=True
            ).stdout.strip()

        return full_path
    except (subprocess.CalledProcessError, FileNotFoundError):
        error_msg = (
            "tgn not found. Please install tgn and ensure it is available "
            "in your PATH."
        )
        raise RuntimeError(error_msg)


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


def build_cxx_extensions(
    root_dir: Path,
    os_type: str = "linux",
    cpu_type: str = "x64",
    build_type: str = "release",
) -> bool:
    """
    Build all C++ extensions using TEN Framework's tgn build tool.

    Args:
        root_dir: Root directory of the project
        os_type: Operating system type (linux, mac, etc.)
        cpu_type: CPU architecture (x64, arm64, etc.)
        build_type: Build type (debug, release)

    Returns:
        True if successful, False otherwise
    """
    print_info("Building C++ extensions...")

    # Check if scripts/BUILD.gn exists
    build_gn_src = root_dir / "scripts" / "BUILD.gn"
    if not build_gn_src.exists():
        print_warning(
            "scripts/BUILD.gn not found. Skipping C++ extensions build."
        )
        return True

    # Copy BUILD.gn to root directory
    build_gn_dst = root_dir / "BUILD.gn"
    try:
        shutil.copy2(build_gn_src, build_gn_dst)
        print_info(f"Copied BUILD.gn to {root_dir}")
    except Exception as e:
        print_error(f"Failed to copy BUILD.gn: {e}")
        return False

    # Find tgn executable
    try:
        tgn_path = find_tgn_executable()
        print_success(f"Found tgn: {tgn_path}")
    except RuntimeError as e:
        print_error(str(e))
        return False

    # Run tgn gen
    try:
        print_info(f"Running tgn gen {os_type} {cpu_type} {build_type}...")

        # Build the command based on OS type
        tgn_gen_cmd = [tgn_path, "gen", os_type, cpu_type, build_type]

        # On macOS, use default clang compiler
        # On other platforms, add custom flags
        if os_type != "mac":
            tgn_gen_cmd.extend(
                [
                    "--",
                    "is_clang=false",
                    "enable_sanitizer=false",
                ]
            )

        subprocess.run(
            tgn_gen_cmd,
            cwd=str(root_dir),
            check=True,
            capture_output=False,
        )
        print_success("tgn gen completed")
    except subprocess.CalledProcessError as e:
        print_error(f"tgn gen failed (exit code: {e.returncode})")
        return False

    # Run tgn build
    try:
        print_info(f"Running tgn build {os_type} {cpu_type} {build_type}...")
        subprocess.run(
            [tgn_path, "build", os_type, cpu_type, build_type],
            cwd=str(root_dir),
            check=True,
            capture_output=False,
        )
        print_success("tgn build completed")
    except subprocess.CalledProcessError as e:
        print_error(f"tgn build failed (exit code: {e.returncode})")
        return False

    # Copy the output of ten_packages to the ten_packages/extension/xx/lib
    out_dir = (
        root_dir / "out" / os_type / cpu_type / "ten_packages" / "extension"
    )
    ten_packages_dir = root_dir / "ten_packages" / "extension"

    if not out_dir.exists():
        print_warning("No C++ extension output directory found")
        return True

    extension_dirs = list(out_dir.iterdir())
    if not extension_dirs:
        print_info("No C++ extension output found")
        return True

    # Copy extension libraries
    for extension_out in extension_dirs:
        if not extension_out.is_dir():
            continue

        extension_name = extension_out.name
        extension_lib = extension_out / "lib"

        if not extension_lib.exists():
            print_warning(f"No output for extension {extension_name}")
            continue

        # Create destination directory
        extension_dst = ten_packages_dir / extension_name / "lib"
        extension_dst.mkdir(parents=True, exist_ok=True)

        # Copy library files
        try:
            for item in extension_lib.iterdir():
                dst_item = extension_dst / item.name
                if item.is_file():
                    shutil.copy2(item, dst_item)
                elif item.is_dir():
                    if dst_item.exists():
                        shutil.rmtree(dst_item)
                    shutil.copytree(item, dst_item)
            print_success(f"Copied libraries for extension {extension_name}")
        except Exception as e:
            print_error(f"Failed to copy libraries for {extension_name}: {e}")
            return False

    print_success("All C++ extensions built and copied successfully")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Build Go, Node.js, and C++ projects for TEN Framework",
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
    parser.add_argument(
        "--skip-cxx",
        action="store_true",
        help="Skip C++ extensions build",
    )
    parser.add_argument(
        "--os",
        type=str,
        default=None,
        help="Operating system type (default: auto-detect)",
    )
    parser.add_argument(
        "--cpu",
        type=str,
        default=None,
        help="CPU architecture (default: auto-detect)",
    )
    parser.add_argument(
        "--build-type",
        type=str,
        default="release",
        help="Build type (default: release)",
    )

    args = parser.parse_args()

    # Auto-detect OS and architecture if not provided
    if args.os is None:
        try:
            args.os = detect_os()
            print_info(f"Auto-detected OS: {args.os}")
        except RuntimeError as e:
            print_error(f"Failed to detect OS: {e}")
            return 1

    if args.cpu is None:
        try:
            args.cpu = detect_arch()
            print_info(f"Auto-detected architecture: {args.cpu}")
        except RuntimeError as e:
            print_error(f"Failed to detect architecture: {e}")
            return 1

    # Get project root directory
    script_dir = Path(__file__).parent
    root_dir = script_dir.parent.resolve()

    print_header("TEN Framework Build")
    print_info(f"Project root: {root_dir}")
    print_info(f"Target platform: {args.os} {args.cpu} {args.build_type}")

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

    # Build C++ extensions
    if not args.skip_cxx:
        print_header("Building C++ Extensions")

        if not build_cxx_extensions(
            root_dir, args.os, args.cpu, args.build_type
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
