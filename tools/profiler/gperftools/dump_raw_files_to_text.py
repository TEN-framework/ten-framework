"""This script is used to convert the raw files(.raw) to text files(.txt)"""

import argparse
import os
import subprocess
import utils.pprof_cmd as pprof_cmd


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.raw_dir: str
        self.text_dir: str


class HeapInfo:
    def __init__(self, raw_file, text_file):
        self.raw_file = raw_file
        self.text_file = text_file


def dump_raw_files_to_text(heap_infos):
    convert_to_text_cmds = []
    for heap_info in heap_infos:
        if os.path.exists(heap_info.text_file) is False:
            convert_to_text_cmds.append(
                pprof_cmd.convert_raw_to_text_cmd(
                    heap_info.raw_file, heap_info.text_file
                )
            )

    # combine all convert_to_raw_cmds to one command
    convert_to_text_cmd = " & ".join(convert_to_text_cmds)

    # run convert_to_text_cmd
    rc = subprocess.run(convert_to_text_cmd, shell=True)
    if rc.returncode != 0:
        raise Exception(
            "convert_to_text_cmd failed, convert_to_text_cmd:"
            f" {convert_to_text_cmd}"
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--raw-dir", type=str, required=True)
    parser.add_argument("--text-dir", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    print(
        f"dump heap in text format start: raw_dir: {args.raw_dir}, text_dir:"
        f" {args.text_dir}"
    )

    if not os.path.exists(args.text_dir):
        os.mkdir(args.text_dir)

    # read raw files and convert to text files
    heap_infos = []
    for root, dirs, files in os.walk(args.raw_dir):
        for file in files:
            if file.endswith(".raw"):
                raw_file_path = os.path.join(root, file)
                heap_infos.append(
                    HeapInfo(
                        raw_file_path,
                        args.text_dir + "/" + file.replace(".raw", ".txt"),
                    )
                )

    print(f"dump heap in text format heap_files size: {len(heap_infos)}")

    dump_raw_files_to_text(heap_infos)
