"""get the total bytes from the pprof text file."""

import re


def get_total_bytes_from_text(filename, output_unit):
    with open(filename, "r", encoding="utf-8") as file:
        data = file.read()

    # Use regular expression to match the total size value
    match = re.search(r"([\d\.]+)([B|K|M|G]{1}[B]{0,1}) TOTAL", data.upper())

    if match:
        value = float(match.group(1))
        unit = match.group(2)

        # Convert the size to B
        if unit == "B":
            value = value
        elif unit == "KB" or unit == "K":
            value *= 1024
        elif unit == "MB" or unit == "M":
            value *= 1024 * 1024
        elif unit == "GB" or unit == "G":
            value *= 1024 * 1024 * 1024

        # Convert the size to the output unit
        if output_unit == "B":
            return value
        elif output_unit == "KB":
            return value / 1024
        elif output_unit == "MB":
            return value / (1024 * 1024)
        elif output_unit == "GB":
            return value / (1024 * 1024 * 1024)
    else:
        print("No total size found in the file.")
        return None
