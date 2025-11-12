#!/bin/bash
#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#

# Check if at least one argument is provided.
if [ "$#" -lt 1 ]; then
  echo "Usage: $0 /path/to/folder [additional paths...]"
  exit 1
fi

# Use a global array for base paths.
BASE_PATHS=("$@")

# Use all input arguments and retrieve immediate sub-folders.
FOLDERS=()

for base_path in "${BASE_PATHS[@]}"; do
  for sub_folder in "$base_path"/*/; do
    # Remove trailing slash.
    sub_folder="${sub_folder%/}"
    # Check if the subfolder already exists in FOLDERS.
    folder_exists=false
    for folder in "${FOLDERS[@]}"; do
      if [[ "$folder" == "$sub_folder" ]]; then
        folder_exists=true
        break
      fi
    done
    if ! $folder_exists; then
      FOLDERS+=("$sub_folder")
      # Initialize the maximum usage for the new subfolder to 0.
      MAX_USAGE_MAP["$sub_folder"]=0
      echo "Detected new sub-folder: $sub_folder"
    fi
  done
done

# Declare an associative array to store the maximum usage for each folder.
declare -A MAX_USAGE_MAP

# Initialize the maximum usage to 0.
for folder in "${FOLDERS[@]}"; do
  MAX_USAGE_MAP["$folder"]=0
done

echo "Disk Usage Monitor:"

while true; do
  echo "----------------------------------------"

  # Re-scan immediate sub-folders under each base path.
  for base_folder in "${BASE_PATHS[@]}"; do
    for sub_folder in "$base_folder"/*/; do
      # Remove trailing slash.
      sub_folder="${sub_folder%/}"
      # Check if the subfolder already exists in FOLDERS.
      folder_exists=false
      for folder in "${FOLDERS[@]}"; do
        if [[ "$folder" == "$sub_folder" ]]; then
          folder_exists=true
          break
        fi
      done
      if ! $folder_exists; then
        FOLDERS+=("$sub_folder")
        # Initialize the maximum usage for the new subfolder to 0.
        MAX_USAGE_MAP["$sub_folder"]=0
        echo "Detected new sub-folder: $sub_folder"
      fi
    done
  done

  mapfile -t sorted_folders < <(printf "%s\n" "${FOLDERS[@]}" | sort)

  for folder in "${sorted_folders[@]}"; do
    # Use the base path to calculate the relative path.
    # Iterate over BASE_PATHS to find the base path of the folder.
    relative_path="$folder"
    for base_path in "${BASE_PATHS[@]}"; do
      if [[ "$folder" == "$base_path/"* ]]; then
        relative_path="${folder#$base_path/}"
        break
      fi
    done

    # Check if the folder exists.
    if [ -d "$folder" ]; then
      # Get the current usage (in KB).
      CURRENT_USAGE_KB=$(du -s "$folder" | awk '{print $1}')

      # Convert KB to GB with two decimal places.
      CURRENT_USAGE_GB=$(echo "scale=2; $CURRENT_USAGE_KB / 1024 / 1024" | bc)

      # Retrieve the previously recorded maximum usage.
      PREV_MAX_USAGE=${MAX_USAGE_MAP["$folder"]}

      # Compare and update the maximum usage.
      IS_NEW_MAX=$(echo "$CURRENT_USAGE_GB > $PREV_MAX_USAGE" | bc -l)
      if [ "$IS_NEW_MAX" -eq 1 ]; then
        MAX_USAGE_MAP["$folder"]=$CURRENT_USAGE_GB
      fi

      # Print current and maximum usage.
      echo "Folder: $relative_path: ${CURRENT_USAGE_GB} (${MAX_USAGE_MAP["$folder"]}) GB"
    else
      # Handle non-existing folders.
      # Print the historical maximum usage and indicate the folder no longer exists.
      echo "Folder: $relative_path: ${MAX_USAGE_MAP["$folder"]} GB (Folder no longer exists)"
    fi
  done

  sleep 1
done
