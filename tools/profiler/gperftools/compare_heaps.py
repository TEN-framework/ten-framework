"""compare two heap file(.heap/.raw) and output the difference"""

import argparse
import subprocess
import utils.pprof_cmd as pprof_cmd


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.base_file: str
        self.target_file: str
        self.output_type: str
        self.output_file: str


def compare_heaps(base_file, target_file, output_type, output_file):
    cmd = pprof_cmd.compare_heaps_cmd(
        base_file, target_file, output_type, output_file
    )
    print(f"compare_heaps_cmd: {cmd}")

    # run cmd
    rc = subprocess.run(cmd, shell=True)
    if rc.returncode != 0:
        raise Exception(f"compare_heaps_cmd failed, cmd: {cmd}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()

    parser.add_argument("--base-file", type=str, required=True)
    parser.add_argument("--target-file", type=str, required=True)
    parser.add_argument("--output-type", type=str, required=True)
    parser.add_argument("--output-file", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    print(
        f"compare heaps start: base_file: {args.base_file}, target_file:"
        f" {args.target_file}, output_type: {args.output_type}, output_file:"
        f" {args.output_file}"
    )

    compare_heaps(
        args.base_file, args.target_file, args.output_type, args.output_file
    )
