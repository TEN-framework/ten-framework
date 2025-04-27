"""This script is used to convert the heap files(.heap) to raw files(.raw)"""

import argparse
import os
import subprocess
import utils.pprof_cmd as pprof_cmd


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.heap_dir: str
        self.bin: str
        self.raw_dir: str


class HeapInfo:
    def __init__(self, heap_file, raw_file):
        self.heap_file = heap_file
        self.raw_file = raw_file


def dump_heap_files_to_raw(bin, heap_infos):
    convert_to_raw_cmds = []
    for heap_info in heap_infos:
        if os.path.exists(heap_info.raw_file) is False:
            convert_to_raw_cmds.append(
                pprof_cmd.convert_heap_to_raw_cmd(
                    bin, heap_info.heap_file, heap_info.raw_file
                )
            )

    # combine all convert_to_raw_cmds to one command
    convert_to_raw_cmd = " & ".join(convert_to_raw_cmds)

    # run convert_to_raw_cmd
    rc = subprocess.run(convert_to_raw_cmd, shell=True)
    if rc.returncode != 0:
        raise Exception(
            "convert_to_raw_cmd failed, convert_to_raw_cmd:"
            f" {convert_to_raw_cmd}"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--heap-dir", type=str, required=True)
    parser.add_argument("--bin", type=str, required=True)
    parser.add_argument("--raw-dir", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    print(
        f"dump heap in raw format start: heap_dir: {args.heap_dir}, bin:"
        f" {args.bin}, raw_dir: {args.raw_dir} "
    )

    if not os.path.exists(args.raw_dir):
        os.mkdir(args.raw_dir)

    # read heap files and convert to text files
    heap_infos = []
    for root, dirs, files in os.walk(args.heap_dir):
        for file in files:
            if file.endswith(".heap"):
                heap_file_path = os.path.join(root, file)
                heap_infos.append(
                    HeapInfo(heap_file_path, args.raw_dir + "/" + file + ".raw")
                )

    print(f"analyze c heap files heap_files size: {len(heap_infos)}")

    dump_heap_files_to_raw(args.bin, heap_infos)
