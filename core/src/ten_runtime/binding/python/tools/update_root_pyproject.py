#!/usr/bin/env python3
#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
"""
This script automatically updates the pyproject.toml file in the root directory.

It scans all Python packages (directories containing pyproject.toml) under the
ten_packages directory:
- Packages under ten_packages/system will be added to [tool.uv.sources] as
  local dependencies
- Packages under ten_packages/extension will be added to [tool.uv.workspace]
  members

If pyproject.toml already exists, only [tool.uv.sources] and [tool.uv.workspace]
sections will be updated.
If pyproject.toml does not exist, a new complete file will be created.
"""

import sys
import io
import argparse
from pathlib import Path
from typing import cast

# To solve Error: 'gbk' codec can't encode character '\u2713' in MinGW environment
# \u2713: ✓ (check sign)
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

try:
    import tomlkit
    from tomlkit.toml_document import TOMLDocument
    from tomlkit.items import Table, InlineTable, Array
except ImportError:
    print(
        "Error: Missing dependency tomlkit\nPlease install: pip install tomlkit",
        file=sys.stderr,
    )
    sys.exit(1)


def find_python_packages(base_dir: Path) -> list[tuple[str, Path]]:
    """
    Find all Python packages containing pyproject.toml in the specified directory.

    Args:
        base_dir: Base directory to search

    Returns:
        List of tuples containing package name and path [(package_name, package_path), ...]
    """
    packages: list[tuple[str, Path]] = []

    if not base_dir.exists():
        return packages

    # Iterate through first-level subdirectories
    for item in base_dir.iterdir():
        if not item.is_dir():
            continue

        pyproject_file = item / "pyproject.toml"
        if pyproject_file.exists():
            # Read package name from pyproject.toml
            try:
                with open(pyproject_file, "r", encoding="utf-8") as f:
                    doc: TOMLDocument = tomlkit.parse(f.read())

                project = doc.get("project")
                if (
                    project is not None
                    and isinstance(project, dict)
                    and "name" in project
                ):
                    package_name = str(project["name"])  # type: ignore[index]
                    packages.append((package_name, item))
            except Exception as e:
                print(
                    f"Warning: Failed to read {pyproject_file}: {e}",
                    file=sys.stderr,
                )
                continue

    return packages


def update_existing_pyproject(
    pyproject_file: Path,
    app_root: Path,
    system_packages: list[tuple[str, Path]],
    extension_packages: list[tuple[str, Path]],
) -> str:
    """
    Update existing pyproject.toml file, only modifying [tool.uv.sources] and [tool.uv.workspace].

    Args:
        pyproject_file: Path to pyproject.toml file
        app_root: Application root directory
        system_packages: List of packages under system directory
        extension_packages: List of packages under extension directory

    Returns:
        Updated pyproject.toml content
    """
    with open(pyproject_file, "r", encoding="utf-8") as f:
        doc: TOMLDocument = tomlkit.parse(f.read())

    # Ensure [tool] section exists
    if "tool" not in doc:
        doc["tool"] = tomlkit.table()

    tool = cast(Table, doc["tool"])

    # Ensure [tool.uv] section exists
    if "uv" not in tool:
        tool["uv"] = tomlkit.table()

    uv = cast(Table, tool["uv"])

    # Update [tool.uv.sources]
    if system_packages:
        sources: Table = tomlkit.table()
        for package_name, package_path in system_packages:
            relative_path = str(package_path.relative_to(app_root))
            # Create inline table
            source_table: InlineTable = tomlkit.inline_table()
            source_table["path"] = relative_path
            source_table["editable"] = True
            sources[package_name] = source_table

        uv["sources"] = sources
    else:
        # If no system packages, remove sources section
        if "sources" in uv:
            del uv["sources"]

    # Update [tool.uv.workspace]
    if extension_packages:
        workspace: Table = tomlkit.table()
        members: Array = tomlkit.array()
        members.multiline(True)  # Use multiline format

        for _, package_path in extension_packages:
            relative_path = str(package_path.relative_to(app_root))
            members.append(relative_path)  # type: ignore[arg-type]

        workspace["members"] = members
        uv["workspace"] = workspace
    else:
        # If no extension packages, remove workspace section
        if "workspace" in uv:
            del uv["workspace"]

    return tomlkit.dumps(doc)


def generate_new_pyproject_content(
    app_root: Path,
    system_packages: list[tuple[str, Path]],
    extension_packages: list[tuple[str, Path]],
) -> str:
    """
    Generate new pyproject.toml content (when file does not exist).

    Args:
        app_root: Application root directory
        system_packages: List of packages under system directory
        extension_packages: List of packages under extension directory

    Returns:
        pyproject.toml file content
    """
    doc: TOMLDocument = tomlkit.document()

    # Create [project] section
    project: Table = tomlkit.table()
    project["name"] = app_root.name
    project["version"] = "0.1.0"
    project["requires-python"] = ">=3.10"

    # Add system packages as dependencies because main.py may import them directly
    # Extension packages should also declare their own dependencies
    if system_packages:
        dependencies: Array = tomlkit.array()
        dependencies.multiline(True)

        for package_name, _ in system_packages:
            dependencies.append(package_name)  # type: ignore[arg-type]

        project["dependencies"] = dependencies

    doc["project"] = project

    # Add a blank line
    doc.add(tomlkit.nl())

    # Create [tool.uv.sources] section
    if system_packages:
        if "tool" not in doc:
            doc["tool"] = tomlkit.table()
        tool = cast(Table, doc["tool"])

        if "uv" not in tool:
            tool["uv"] = tomlkit.table()
        uv = cast(Table, tool["uv"])

        sources: Table = tomlkit.table()
        for package_name, package_path in system_packages:
            relative_path = str(package_path.relative_to(app_root))
            source_table: InlineTable = tomlkit.inline_table()
            source_table["path"] = relative_path
            source_table["editable"] = True
            sources[package_name] = source_table

        uv["sources"] = sources

    # Add [tool.uv.workspace] section
    if extension_packages:
        if "tool" not in doc:
            doc["tool"] = tomlkit.table()
        tool = cast(Table, doc["tool"])

        if "uv" not in tool:
            tool["uv"] = tomlkit.table()
        uv = cast(Table, tool["uv"])

        workspace: Table = tomlkit.table()
        members: Array = tomlkit.array()
        members.multiline(True)

        for _, package_path in extension_packages:
            relative_path = str(package_path.relative_to(app_root))
            members.append(relative_path)  # type: ignore[arg-type]

        workspace["members"] = members
        uv["workspace"] = workspace

    return tomlkit.dumps(doc)  # type: ignore[arg-type]


def update_root_pyproject(app_root: Path, dry_run: bool = False) -> bool:
    """
    Update the pyproject.toml file in the root directory.

    Args:
        app_root: Application root directory (containing ten_packages directory)
        dry_run: If True, only print content without writing to file

    Returns:
        True if successful, False otherwise
    """
    ten_packages_dir = app_root / "ten_packages"

    if not ten_packages_dir.exists():
        print(
            f"Error: ten_packages directory does not exist: {ten_packages_dir}",
            file=sys.stderr,
        )
        return False

    # Find Python packages under system and extension directories
    system_dir = ten_packages_dir / "system"
    extension_dir = ten_packages_dir / "extension"

    system_packages = find_python_packages(system_dir)
    extension_packages = find_python_packages(extension_dir)

    print(f"Found {len(system_packages)} system package(s):")
    for name, path in system_packages:
        print(f"  - {name} ({path.relative_to(app_root)})")

    print(f"\nFound {len(extension_packages)} extension package(s):")
    for name, path in extension_packages:
        print(f"  - {name} ({path.relative_to(app_root)})")

    pyproject_file = app_root / "pyproject.toml"

    # Check if file exists
    try:
        if pyproject_file.exists():
            print(
                "\nDetected existing pyproject.toml, "
                + "will only update [tool.uv.sources] and [tool.uv.workspace]"
            )
            content = update_existing_pyproject(
                pyproject_file, app_root, system_packages, extension_packages
            )
        else:
            print("\npyproject.toml not found, will create new file")
            content = generate_new_pyproject_content(
                app_root, system_packages, extension_packages
            )
    except Exception as e:
        print(f"Error: Failed to process file: {e}", file=sys.stderr)
        return False

    if dry_run:
        print("\n" + "=" * 60)
        print("Preview mode - pyproject.toml content:")
        print("=" * 60)
        print(content)
        print("=" * 60)
        return True

    # Write to file
    try:
        with open(pyproject_file, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"\n✓ Successfully updated: {pyproject_file}")
        return True
    except Exception as e:
        print(
            f"Error: Failed to write file {pyproject_file}: {e}",
            file=sys.stderr,
        )
        return False


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Update the pyproject.toml file in the application root directory, "
            "automatically configure uv workspace and sources"
        )
    )
    parser.add_argument(
        "app_root",
        nargs="?",
        default=".",
        help="Application root directory path (defaults to current directory)",
    )
    parser.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        help="Preview mode, only display content without writing to file",
    )

    args = parser.parse_args()

    app_root = Path(str(args.app_root)).resolve()

    if not app_root.exists():
        print(f"Error: Directory does not exist: {app_root}", file=sys.stderr)
        return 1

    print(f"Application root directory: {app_root}\n")

    success = update_root_pyproject(app_root, bool(args.dry_run))
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
