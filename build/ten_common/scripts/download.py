#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import sys
import requests


def download_file(url: str, output_path: str) -> None:
    response = requests.get(url, stream=True, timeout=60)  # 60 seconds timeout.
    response.raise_for_status()
    with open(output_path, "wb") as file:
        for chunk in response.iter_content(chunk_size=8192):
            file.write(chunk)


if __name__ == "__main__":
    download_file(sys.argv[1], sys.argv[2])
