#!/usr/bin/env python3
"""
Script to upgrade the log configuration in property.json from version 0.10 to 0.11.
This script converts the old log configuration format to the new format.

The script handles two types of old configurations:
1. Root level configuration:
   - log_level and log_file fields directly under ten
2. Log object configuration:
   - log object with level and file fields under ten
"""

from typing import TypedDict, cast
import json
import sys
import os


class LogHandler(TypedDict):
    """Type definition for log handler configuration."""

    matchers: list[dict[str, str]]
    formatter: dict[str, bool | str]
    emitter: dict[str, dict[str, str] | str]


class LogConfig(TypedDict):
    """Type definition for log configuration."""

    handlers: list[LogHandler]


class OldLogConfig(TypedDict, total=False):
    """Type definition for old log configuration."""

    level: int
    file: str
    encryption: dict[str, bool | str]


class TenConfig(TypedDict, total=False):
    """Type definition for ten configuration."""

    log_level: int
    log_file: str
    log: OldLogConfig | LogConfig


class PropertyJson(TypedDict, total=False):
    """Type definition for property.json content."""

    ten: TenConfig


def convert_log_level(old_level: int) -> str:
    """Convert old numeric log level to new string format.

    Args:
        old_level: Old numeric log level (1-6)

    Returns:
        str: New string format log level
    """
    level_map = {
        1: "debug",
        2: "debug",
        3: "info",
        4: "warn",
        5: "error",
        6: "error",
    }
    return level_map.get(old_level, "info")


def create_new_log_config(
    old_level: int | None = None, old_file: str | None = None
) -> LogConfig:
    """Create new log configuration based on old settings.

    Args:
        old_level: Old numeric log level (optional)
        old_file: Old log file path (optional)

    Returns:
        LogConfig: New log configuration structure
    """
    level = convert_log_level(old_level) if old_level else "info"

    # Basic handler structure
    handler: LogHandler = {
        "matchers": [{"level": level}],
        "formatter": {"type": "plain", "colored": True},
        "emitter": (
            {"type": "file", "config": {"path": old_file}}
            if old_file
            else {"type": "console", "config": {"stream": "stdout"}}
        ),
    }

    return {"handlers": [handler]}


def upgrade_property_json(file_path: str) -> bool:
    """Upgrade the specified property.json file.

    Args:
        file_path: Path to the property.json file

    Returns:
        bool: True if upgrade successful or not needed, False if error occurred
    """
    try:
        with open(file_path, "r", encoding="utf-8") as file:
            data = cast(PropertyJson, json.load(file))
    except json.JSONDecodeError as error:
        print(f"Error: Cannot parse JSON file {file_path}: {error}")
        return False
    except FileNotFoundError:
        print(f"Error: File not found {file_path}")
        return False

    modified = False

    # Check and update ten field
    if "ten" in data:
        ten_config = data["ten"]

        # First check root level configuration
        root_level = ten_config.get("log_level")
        root_file = ten_config.get("log_file")

        # Then check log object configuration
        old_log = ten_config.get("log", {})
        if "level" in old_log or "file" in old_log:
            # We found old style log configuration in log object
            log_level = old_log.get("level")
            log_file = old_log.get("file")

            # Create new configuration
            new_log_config = create_new_log_config(log_level, log_file)

            # Update with new configuration
            ten_config["log"] = new_log_config
            modified = True

        elif root_level is not None or root_file is not None:
            # Create new configuration from root level settings
            new_log_config = create_new_log_config(root_level, root_file)

            # Remove old configuration
            ten_config.pop("log_level", None)
            ten_config.pop("log_file", None)

            # Add new configuration
            ten_config["log"] = new_log_config
            modified = True

    if modified:
        # Backup original file
        backup_path = file_path + ".bak"
        try:
            os.rename(file_path, backup_path)
            print(f"Created backup file: {backup_path}")
        except OSError as error:
            print(f"Warning: Cannot create backup file: {error}")

        # Write updated configuration
        try:
            with open(file_path, "w", encoding="utf-8") as file:
                json.dump(data, file, indent=2, ensure_ascii=False)
            print(f"Successfully updated file: {file_path}")
            return True
        except OSError as error:
            print(f"Error: Cannot write updated file {file_path}: {error}")
            # Try to restore backup
            if os.path.exists(backup_path):
                try:
                    os.rename(backup_path, file_path)
                    print("Restored original file")
                except OSError:
                    print("Warning: Cannot restore original file")
            return False
    else:
        print(f"File {file_path} does not need update")
        return True


def main() -> None:
    """Main entry point of the script."""
    if len(sys.argv) < 2:
        print("Usage: upgrade_log_config.py <path_to_property.json>")
        sys.exit(1)

    file_path = sys.argv[1]
    if not upgrade_property_json(file_path):
        sys.exit(1)


if __name__ == "__main__":
    main()
