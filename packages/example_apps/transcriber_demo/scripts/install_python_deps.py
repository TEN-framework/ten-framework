#!/usr/bin/env python3.10
"""
Python Dependencies Installation Script

This script will:
1. Execute deps_resolver.py to merge all requirements.txt files
2. Install the merged dependencies to Python 3.10 environment

Usage:
    python3.10 scripts/install_python_deps.py
    python3.10 scripts/install_python_deps.py --index-url https://pypi.org/simple
"""

import argparse
import os
import subprocess
import sys


def get_python310_executable():
    """Find and return the Python 3.10 executable path."""
    # Try common Python 3.10 executable names
    python_names = ['python3.10', 'python310']
    
    for name in python_names:
        try:
            # Check if the command exists and is Python 3.10
            result = subprocess.run(
                [name, '--version'],
                capture_output=True,
                text=True,
                check=True
            )
            version_output = result.stdout.strip()
            if '3.10' in version_output:
                # Get full path
                full_path = subprocess.run(
                    ['which', name],
                    capture_output=True,
                    text=True,
                    check=True
                ).stdout.strip()
                return full_path
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
    
    # If not found, raise an error
    raise RuntimeError(
        "Python 3.10 not found. Please install Python 3.10 and ensure "
        "'python3.10' is available in your PATH."
    )


def main():
    parser = argparse.ArgumentParser(
        description="Resolve and install Python dependencies using Python 3.10"
    )
    parser.add_argument(
        "-i",
        "--index-url",
        type=str,
        required=False,
        default="",
        help="Specify pip index URL (e.g., https://pypi.org/simple)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=False,
        default="merged_requirements.txt",
        help="Output filename for merged requirements (default: merged_requirements.txt)",
    )
    
    args = parser.parse_args()

    # Get Python 3.10 executable
    try:
        python310_path = get_python310_executable()
        print(f"✓ Found Python 3.10: {python310_path}")
    except RuntimeError as e:
        print(f"❌ Error: {e}")
        return 1

    # Get project root directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    app_root_dir = os.path.abspath(os.path.join(script_dir, ".."))
    
    # Path to deps_resolver.py
    deps_resolver_path = os.path.join(
        app_root_dir,
        "ten_packages",
        "system",
        "ten_runtime_python",
        "tools",
        "deps_resolver.py"
    )
    
    # Check if file exists
    if not os.path.exists(deps_resolver_path):
        print(f"❌ Error: deps_resolver.py not found")
        print(f"   Expected path: {deps_resolver_path}")
        return 1
    
    print("=" * 60)
    print("🔧 Step 1/2: Resolving and merging Python dependencies...")
    print("=" * 60)
    
    # Build command arguments
    resolver_args = [
        python310_path,
        deps_resolver_path,
        "--root", app_root_dir,
        "--output", args.output,
    ]
    
    if args.index_url:
        resolver_args.extend(["--index-url", args.index_url])
    
    # Execute deps_resolver.py
    try:
        result = subprocess.run(
            resolver_args,
            cwd=app_root_dir,
            check=True,
            capture_output=False,
        )
        print("✅ Dependency resolution completed")
    except subprocess.CalledProcessError as e:
        print(f"❌ Dependency resolution failed with exit code: {e.returncode}")
        return 1
    except Exception as e:
        print(f"❌ Error executing deps_resolver: {e}")
        return 1
    
    # Check if output file was generated
    merged_requirements_path = os.path.join(app_root_dir, args.output)
    if not os.path.exists(merged_requirements_path):
        print(f"❌ Error: Merged requirements file not found: {args.output}")
        return 1
    
    print("\n" + "=" * 60)
    print("📦 Step 2/2: Installing Python dependencies...")
    print("=" * 60)
    
    # Build pip install command using Python 3.10
    pip_args = [
        python310_path,
        "-m",
        "pip",
        "install",
        "-r",
        args.output,
    ]
    
    if args.index_url:
        pip_args.extend(["-i", args.index_url])
    
    # Execute pip install
    try:
        result = subprocess.run(
            pip_args,
            cwd=app_root_dir,
            check=True,
            capture_output=False,
        )
        print("\n✅ Dependencies installed successfully")
    except subprocess.CalledProcessError as e:
        print(f"\n❌ Dependency installation failed with exit code: {e.returncode}")
        return 1
    except Exception as e:
        print(f"\n❌ Error executing pip install: {e}")
        return 1
    
    print("\n" + "=" * 60)
    print("🎉 All operations completed successfully!")
    print("=" * 60)
    print(f"Merged requirements file: {args.output}")
    print(f"Python 3.10 environment: {python310_path}")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())

