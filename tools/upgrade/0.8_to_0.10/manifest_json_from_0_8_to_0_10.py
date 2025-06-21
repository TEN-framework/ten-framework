#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
import sys
import argparse
from typing import Dict, Any, Optional, List


def convert_property_format(
    old_property: Optional[Dict[str, Any]], old_required: Optional[List[str]]
) -> Optional[Dict[str, Any]]:
    """
    Convert old property format to new format.

    Old format:
    {
      "property": {...},
      "required": [...]
    }

    New format:
    {
      "property": {
        "properties": {...},
        "required": [...]
      }
    }
    """
    if old_property is None and old_required is None:
        return None

    new_property = {}

    if old_property is not None:
        new_property["properties"] = old_property

    if old_required is not None:
        new_property["required"] = old_required

    return new_property


def convert_cmd_like_api(old_cmd: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convert command-like API from old format to new format.
    """
    new_cmd = {"name": old_cmd["name"]}

    # Convert property format
    old_property = old_cmd.get("property")
    old_required = old_cmd.get("required")
    new_property = convert_property_format(old_property, old_required)

    if new_property is not None:
        new_cmd["property"] = new_property

    # Convert result format if present
    if "result" in old_cmd:
        old_result = old_cmd["result"]
        old_result_property = old_result.get("property")
        old_result_required = old_result.get("required")
        new_result_property = convert_property_format(
            old_result_property, old_result_required
        )

        if new_result_property is not None:
            new_cmd["result"] = {"property": new_result_property}

    return new_cmd


def convert_data_like_api(old_data: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convert data-like API from old format to new format.
    """
    new_data = {"name": old_data["name"]}

    # Convert property format
    old_property = old_data.get("property")
    old_required = old_data.get("required")
    new_property = convert_property_format(old_property, old_required)

    if new_property is not None:
        new_data["property"] = new_property

    return new_data


def convert_manifest_api(old_api: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convert the entire API section from old format to new format.
    """
    new_api = {}

    # Convert top-level property
    if "property" in old_api or "required" in old_api:
        old_property = old_api.get("property")
        old_required = old_api.get("required")
        new_property = convert_property_format(old_property, old_required)

        if new_property is not None:
            new_api["property"] = new_property

    # Convert interface (no change needed)
    if "interface" in old_api:
        new_api["interface"] = old_api["interface"]

    # Convert cmd_in/cmd_out
    for cmd_type in ["cmd_in", "cmd_out"]:
        if cmd_type in old_api:
            new_api[cmd_type] = [
                convert_cmd_like_api(cmd) for cmd in old_api[cmd_type]
            ]

    # Convert data_in/data_out
    for data_type in ["data_in", "data_out"]:
        if data_type in old_api:
            new_api[data_type] = [
                convert_data_like_api(data) for data in old_api[data_type]
            ]

    # Convert frame types (audio_frame_in/out, video_frame_in/out)
    for frame_type in [
        "audio_frame_in",
        "audio_frame_out",
        "video_frame_in",
        "video_frame_out",
    ]:
        if frame_type in old_api:
            new_api[frame_type] = [
                convert_data_like_api(frame) for frame in old_api[frame_type]
            ]

    return new_api


def convert_manifest_file(old_manifest: Dict[str, Any]) -> Dict[str, Any]:
    """
    Convert an entire manifest file from old format to new format.
    """
    new_manifest = old_manifest.copy()

    if "api" in old_manifest:
        new_manifest["api"] = convert_manifest_api(old_manifest["api"])

    return new_manifest


def upgrade_manifest_file(file_path: str, dry_run: bool = False) -> bool:
    """
    Upgrade a single manifest.json file.

    Returns True if the file was modified, False otherwise.
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            old_manifest = json.load(f)

        new_manifest = convert_manifest_file(old_manifest)

        # Check if there are any changes
        if old_manifest == new_manifest:
            print(f"No changes needed for {file_path}")
            return False

        if dry_run:
            print(f"Would upgrade {file_path}")
        else:
            # Create backup
            backup_path = file_path + ".bak"
            with open(backup_path, "w", encoding="utf-8") as f:
                json.dump(old_manifest, f, indent=2, ensure_ascii=False)

            # Write new format
            with open(file_path, "w", encoding="utf-8") as f:
                json.dump(new_manifest, f, indent=2, ensure_ascii=False)

            print(f"Upgraded {file_path} (backup: {backup_path})")

        return True

    except Exception as e:
        print(f"Error processing {file_path}: {e}", file=sys.stderr)
        return False


def find_manifest_files(root_dir: str) -> List[str]:
    """
    Find all manifest.json files recursively.
    """
    manifest_files = []

    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file == "manifest.json":
                manifest_files.append(os.path.join(root, file))

    return manifest_files


def main():
    parser = argparse.ArgumentParser(
        description="Upgrade manifest.json files from 0.8 format to 0.10 format"
    )
    parser.add_argument(
        "path",
        help="Path to manifest.json file or directory to search recursively",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be changed without making actual changes",
    )

    args = parser.parse_args()

    if os.path.isfile(args.path):
        # Single file
        manifest_files = [args.path]
    elif os.path.isdir(args.path):
        # Directory - find all manifest.json files
        manifest_files = find_manifest_files(args.path)
    else:
        print(
            f"Error: {args.path} is not a valid file or directory",
            file=sys.stderr,
        )
        return 1

    if not manifest_files:
        print("No manifest.json files found")
        return 0

    total_files = len(manifest_files)
    changed_files = 0

    print(f"Processing {total_files} manifest.json file(s)...")

    for file_path in manifest_files:
        if upgrade_manifest_file(file_path, args.dry_run):
            changed_files += 1

    dry_run_text = "would be " if args.dry_run else ""
    print(
        f"\nCompleted: {changed_files}/{total_files} files were "
        f"{dry_run_text}modified"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
