"""Dump all heap info to excel file."""

import argparse
import os
import time
import utils.get_total_bytes as get_total_bytes
import openpyxl
import dump_heap_files_to_text


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.heap_dir: str
        self.output: str
        self.sample_interval: int


class HeapInfo:
    def __init__(self, heap_file, text_file, total_bytes):
        self.heap_file = heap_file
        self.text_file = text_file
        self.total_bytes = total_bytes


def write_to_excel(heap_infos, output_file, sample_interval):
    # Create the excel file
    workbook = openpyxl.Workbook()
    sheet = workbook.create_sheet(
        index=0, title="Go Heap profile"
    )  # create a new sheet
    workbook.active = sheet  # set the new sheet as the active sheet

    # Write the header to the excel file.
    sheet["A1"] = "index"
    sheet["B1"] = "time/s"
    sheet["C1"] = "heap_file_name"
    sheet["D1"] = "text_file_name"
    sheet["E1"] = "total_heap_size/MB"

    data = []
    for index, heap_info in enumerate(heap_infos):
        data.append(
            [
                index,
                index * sample_interval,
                heap_info.heap_file,
                heap_info.text_file,
                heap_info.total_bytes,
            ]
        )

    # Write the data to the excel file.
    for row in data:
        sheet.append(row)

    # Save the excel file.
    workbook.save(output_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--heap-dir", type=str, required=True)
    parser.add_argument("--output", type=str, required=True)
    parser.add_argument("--sample-interval", type=int, default=30)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    print(
        f"dump heap to excel start: heap_dir: {args.heap_dir}, output:"
        f" {args.output}"
    )

    text_dir = args.heap_dir + "/text"
    if not os.path.exists(text_dir):
        os.mkdir(text_dir)

    # read heap files and convert to text files
    heap_infos = []
    for root, dirs, files in os.walk(args.heap_dir):
        for file in files:
            if file.endswith(".out"):
                heap_file_path = os.path.join(root, file)
                text_file_path = text_dir + "/" + file + ".txt"
                heap_infos.append(HeapInfo(heap_file_path, text_file_path, 0))

    # Sort heap_infos by heap_file name.
    heap_infos.sort(key=lambda heap_info: heap_info.heap_file)

    # Convert heap files to text files.
    dump_heap_files_to_text.dump_heap_files_to_text(heap_infos)

    # wait for 5 seconds to make sure all txt files are generated.
    time.sleep(5)

    # call get_total_bytes to get total bytes
    for heap_info in heap_infos:
        totalBytes = get_total_bytes.get_total_bytes_from_text(
            heap_info.text_file, "MB"
        )
        heap_info.total_bytes = totalBytes

    # If the directory of the output file is not exist, create it.
    output_dir = os.path.dirname(args.output)
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)

    # If the name of the output file does not end with .xlsx, append .xlsx to
    # the name of the output file.
    if not args.output.endswith(".xlsx"):
        args.output = args.output + ".xlsx"

    # Write the result to an excel file.
    write_to_excel(heap_infos, args.output, args.sample_interval)
