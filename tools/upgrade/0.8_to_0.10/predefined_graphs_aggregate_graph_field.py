#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import json
import os
import sys
import copy
from typing import Any


def convert_property_file(old_property: dict[str, Any]) -> dict[str, Any]:
    """
    Aggregate graph field in predefined_graphs field in property.json
    """
    new_property = copy.deepcopy(old_property)

    if "_ten" not in new_property and "ten" not in new_property:
        return new_property

    # Get the correct field name (_ten or ten)
    field_name = "_ten" if "_ten" in new_property else "ten"

    # The predefined_graphs field would be in "_ten" or "ten" field
    predefined_graphs = new_property[field_name].get("predefined_graphs", [])
    if not predefined_graphs:
        return new_property

    for graph in predefined_graphs:
        # Aggregate 'nodes', 'connections', 'exposed_messages', 'exposed_properties', 'import_uri' fields to a new created 'graph' field
        graph["graph"] = {}

        # Only include fields that exist in the original graph
        if "nodes" in graph:
            graph["graph"]["nodes"] = graph.pop("nodes")
        if "connections" in graph:
            graph["graph"]["connections"] = graph.pop("connections")
        if "exposed_messages" in graph:
            graph["graph"]["exposed_messages"] = graph.pop("exposed_messages")
        if "exposed_properties" in graph:
            graph["graph"]["exposed_properties"] = graph.pop(
                "exposed_properties"
            )
        if "import_uri" in graph:
            graph["graph"]["import_uri"] = graph.pop("import_uri")

    return new_property


def upgrade_json_file(file_path: str, dry_run: bool = False) -> bool:
    """
    Upgrade a single JSON file if it matches the criteria.

    Returns True if the file was modified, False otherwise.
    """
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            old_property = json.load(f)

        if not is_target_json_file(old_property):
            return False

        new_property = convert_property_file(old_property)

        # Check if there are any changes
        if old_property == new_property:
            print(f"No changes needed for {file_path}")
            return False

        if dry_run:
            print(f"Would upgrade {file_path}")
        else:
            # Create backup
            backup_path = file_path + ".bak"
            with open(backup_path, "w", encoding="utf-8") as f:
                json.dump(old_property, f, indent=2, ensure_ascii=False)

            # Write new format
            with open(file_path, "w", encoding="utf-8") as f:
                json.dump(new_property, f, indent=2, ensure_ascii=False)

            print(f"Upgraded {file_path} (backup: {backup_path})")

        return True

    except Exception as e:
        print(f"Error processing {file_path}: {e}", file=sys.stderr)
        return False


def is_target_json_file(json_data: dict[str, Any]) -> bool:
    """
    Check if a JSON file matches the criteria for processing:
    - Must have predefined_graphs field in "_ten" or "ten" field
    - predefined_graphs must be a list
    """

    # Check if the json has "_ten" or "ten" field
    if "_ten" not in json_data and "ten" not in json_data:
        return False

    ten_field = json_data.get("_ten", json_data.get("ten", {}))
    predefined_graphs = ten_field.get("predefined_graphs", [])

    if not predefined_graphs:
        return False

    # Check if predefined_graphs is a list
    if not isinstance(predefined_graphs, list):
        return False

    # Check if any graph in predefined_graphs has fields that need to be moved
    for graph in predefined_graphs:
        fields_to_check = [
            "nodes",
            "connections",
            "exposed_messages",
            "exposed_properties",
            "import_uri",
        ]
        if any(field in graph for field in fields_to_check):
            return True

    return False


def find_json_files(root_dir: str) -> list[str]:
    """
    Find all JSON files recursively.
    """
    json_files = []

    for root, _, files in os.walk(root_dir):
        for file in files:
            if file.endswith(".json"):
                json_files.append(os.path.join(root, file))

    return json_files


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Aggregate graph field in predefined_graphs field in property.json"
        )
    )

    parser.add_argument(
        "directory",
        help="Directory to search recursively for JSON files",
    )

    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Show what would be changed without making actual changes",
    )

    args = parser.parse_args()

    if not os.path.isdir(args.directory):
        print(
            f"Error: {args.directory} is not a valid directory",
            file=sys.stderr,
        )
        return 1

    # Find all JSON files
    json_files = find_json_files(args.directory)

    if not json_files:
        print("No JSON files found")
        return 0

    total_files = len(json_files)
    changed_files = 0
    processed_files = 0

    print(
        f"Found {total_files} JSON file(s), "
        "checking which ones need processing..."
    )

    for file_path in json_files:
        if upgrade_json_file(file_path, args.dry_run):
            changed_files += 1
            processed_files += 1
        else:
            # Check if it was skipped due to criteria or no changes needed
            try:
                with open(file_path, "r", encoding="utf-8") as f:
                    json_data = json.load(f)
                if is_target_json_file(json_data):
                    processed_files += 1
            except Exception as e:
                print(f"Error processing {file_path}: {e}", file=sys.stderr)

    dry_run_text = "would be " if args.dry_run else ""
    print(
        f"\nCompleted: {processed_files} files matched criteria, "
        f"{changed_files} files were {dry_run_text}modified"
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
