#!/usr/bin/env python
# -*- coding: utf-8 -*-

import json
import os
import sys
import subprocess
import shutil
import platform
from typing import List, Tuple
import argparse


VERSION = "0.1.0"

# The script determines the directory of the script itself and constructs paths
# for .gnfiles/, bin/, and specific executables (gn and ninja) based on the
# operating system.


class MainArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()
        self.verbose: bool
        self.build_command: str
        self.build_target: str
        self.target_os: str
        self.target_cpu: str
        self.build_type: str
        self.out_dir: str
        self.out_file: str


class AllArgumentInfo(MainArgumentInfo):
    def __init__(self):
        super().__init__()
        self.script_path: str
        self.gn_path: str
        self.ninja_path: str
        self.generator: str
        self.extra_args_list: list[str]


class CommandAction(argparse.Action):
    def __init__(self, option_strings, dest, nargs=None, **kwargs):
        super().__init__(option_strings, dest, **kwargs)

    def __call__(self, parser, namespace, values: str, option_string=None):
        separator = values.find(":")
        if separator != -1:
            build_command = values[0:separator]
            build_target = values[(separator + 1) :]  # noqa
        else:
            build_command = values
            build_target = ""

        validate_build_command(build_command)

        setattr(namespace, "build_command", build_command)
        setattr(namespace, "build_target", build_target)


def merge(ob1, ob2):
    """an object's __dict__ contains all its attributes, methods, docstrings,
    etc.
    """

    ob1.__dict__.update(ob2.__dict__)
    return ob1


def create_all_args() -> AllArgumentInfo:
    return AllArgumentInfo()


def determine_essential_paths(all_args: AllArgumentInfo) -> None:
    # Get the directory of the script.
    all_args.script_path = os.path.dirname(os.path.realpath(__file__))

    # Get the path of '.gnfiles/'
    all_args.script_path = os.path.join(all_args.script_path, ".gnfiles")

    all_args.gn_path = os.path.join(all_args.script_path, "bin")
    all_args.ninja_path = os.path.join(all_args.script_path, "bin")

    if sys.platform == "win32":
        machine = platform.machine().lower()
        if machine in ["arm64", "aarch64"]:
            arch_folder = "arm64"
        elif machine in ["amd64", "x86_64"]:
            arch_folder = "x64"
        else:
            raise ValueError(f"Unsupported architecture: {machine}")

        all_args.gn_path = os.path.join(
            all_args.gn_path, "win", arch_folder, "gn.exe"
        )
        all_args.ninja_path = os.path.join(
            all_args.ninja_path, "win", arch_folder, "ninja.exe"
        )
    elif sys.platform == "darwin":
        # The ninja and gn binaries for Mac support both x64 and arm64 architectures.
        all_args.gn_path = os.path.join(all_args.gn_path, "mac", "gn")
        all_args.ninja_path = os.path.join(all_args.ninja_path, "mac", "ninja")
    else:
        if os.uname().machine in ["arm64", "aarch64"]:
            all_args.gn_path = os.path.join(
                all_args.gn_path, "linux", "arm64", "gn"
            )
            all_args.ninja_path = os.path.join(
                all_args.ninja_path, "linux", "arm64", "ninja"
            )
        else:
            all_args.gn_path = os.path.join(
                all_args.gn_path, "linux", "x64", "gn"
            )
            all_args.ninja_path = os.path.join(
                all_args.ninja_path, "linux", "x64", "ninja"
            )

    # 'gn_path' is the path of the 'gn' executable.
    all_args.gn_path = os.path.abspath(all_args.gn_path).replace("\\", "/")

    # 'ninja_path' is the path of the 'ninja' executable.
    all_args.ninja_path = os.path.abspath(all_args.ninja_path).replace(
        "\\", "/"
    )


def check_python_version() -> None:
    """Checks if the Python version is 3 or above."""

    if sys.version_info.major < 3:
        err_msg = (
            f"Your python version is {sys.version_info},"
            " please change to python3."
        )
        print(err_msg)
        sys.exit(-1)


def run_cmd(cmd: str, show_output: bool = True, echo: bool = False) -> int:
    """Execute a command in a shell.

    Args:
        cmd (str): Command to execute.
        show_output (bool): Whether to show the output of the command.
        echo (bool): Whether to echo the command.

    Returns:
        int: Return code of the command.
    """

    if echo:
        print(f">>> {cmd}")

    dev_null = open(os.devnull, "w", encoding="utf-8")
    if show_output:
        stderr = sys.stderr
        stdout = sys.stdout
    else:
        stderr = dev_null
        stdout = dev_null

    pipe = subprocess.Popen(cmd, stderr=stderr, stdout=stdout, shell=True)
    ret = pipe.wait()
    dev_null.close()
    return ret


def run_or_die(cmd: str, show_output: bool = True, echo: bool = False) -> None:
    """Runs a command and exits if it fails.

    Args:
        cmd: The command to run.
        show_output: Whether to show the output from the command.
        echo: Whether to echo the command before running it.
    """

    if run_cmd(cmd, show_output, echo) != 0:
        sys.exit(-1)


def run_not_die(cmd: str, show_output: bool = True, echo: bool = False) -> None:
    """Run a command, and raise an error if it fails.

    Args:
        cmd: The command to run.
        show_output: Whether to show the output from the command.
        echo: Whether to echo the command before running it.
    """
    run_cmd(cmd, show_output, echo)


def run_and_redirect_output(cmd: str, output_file: str) -> None:
    """Run a command and redirect its output to a file.

    Args:
        cmd: The command to run.
        output_file: The file to which to redirect the output.
    """
    with open(output_file, "w", encoding="utf-8") as f:
        subprocess.run(cmd, shell=True, stdout=f, stderr=f, check=True)


def get_cmd_output(cmd: str, echo: bool = False) -> Tuple[int, str]:
    """Executes cmd in a shell and returns its status and output.

    Args:
        cmd (str): The command to execute.

    Returns:
        Tuple[int, str]: A tuple of the command's status and output.
    """

    if sys.platform != "win32":
        cmd = "{ " + cmd + "; }"

    if echo:
        print(f">>> {cmd}")

    pipe = os.popen(cmd + " 2>&1", "r")
    output = ""

    while 1:
        line = pipe.readline()
        if not line:
            break
        output += line

    try:
        status = pipe.close()
    except Exception:
        status = -1

    if status is None:
        status = 0

    if output[-1:] == "\n":
        output = output[:-1]

    return status, output


def validate_build_command(build_command: str) -> None:
    if build_command not in [
        "build",
        "clean",
        "desc",
        "explain_build",
        "gen",
        "graph",
        "rebuild",
        "show_deps",
        "show_input_output",
        "show_input",
        "uninstall",
        "path",
        "refs",
        "check",
        "args",
    ]:
        print(f"\n Invalid command '{build_command}'\n")
        exit(-1)


def validate_cpu(target_cpu: str, target_os: str) -> None:
    """Validates that the target CPU is valid for the target OS."""

    valid_os_cpu_mapping = {
        "win": ["x86", "x64", "arm64"],
        "linux": ["x86", "x64", "arm", "arm64"],
        "mac": ["x64", "arm64"],
    }

    if target_os not in valid_os_cpu_mapping:
        print(
            f"\n Can not build arch with name {target_cpu} in OS {target_os}\n"
        )
        exit(-1)

    if target_cpu not in valid_os_cpu_mapping[target_os]:
        print(
            f"\n Can not build arch with name {target_cpu} in OS {target_os}\n"
        )
        exit(-1)


def parse_main_args(
    all_args: AllArgumentInfo, main_args_list: list[str]
) -> None:
    """Analyze the command line arguments after the 'tgn' command.

    For example, 'tgn -h' will analyze '-h'.
    """

    main_args_parser = argparse.ArgumentParser(
        prog="tgn",
        description="An easy-to-use Google gn wrapper",
        epilog=(
            f"I recommend you to put {all_args.script_path} into your PATH so"
            " that you can run tgn anywhere."
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    main_args_parser.add_argument(
        "-v",
        "--version",
        action="version",
        version=VERSION,
    )
    main_args_parser.add_argument(
        "--verbose",
        dest="verbose",
        help="dump verbose outputs",
        action="store_true",
        default=False,
    )
    main_args_parser.add_argument(
        "--out-dir",
        help="build output dir, default is 'out/'",
        type=str,
        required=False,
        default="out",
    )
    main_args_parser.add_argument(
        "command",
        metavar="command",
        help=(
            "possible commands are:\n"
            "gen         build        rebuild            refs    clean\n"
            "graph       uninstall    explain_build      desc    check\n"
            "show_deps   show_input   show_input_output  path    args"
        ),
        type=str,
        action=CommandAction,
    )
    main_args_parser.add_argument(
        "target_os",
        metavar="target-OS",
        help="possible OS values are:\nwin   mac   linux",
        type=str,
        choices={"win", "mac", "linux"},
    )
    main_args_parser.add_argument(
        "target_cpu",
        metavar="target-CPU",
        help="possible values are:\nx86   x64   arm   arm64",
        type=str,
        choices={"x86", "x64", "arm", "arm64"},
    )
    main_args_parser.add_argument(
        "build_type",
        metavar="build-type",
        help="possible values are:\ndebug   release",
        type=str,
        choices={"debug", "release"},
    )

    arg_info = MainArgumentInfo()
    main_args = main_args_parser.parse_args(main_args_list, namespace=arg_info)

    validate_cpu(main_args.target_cpu, main_args.target_os)

    # Append OS and CPU into out_dir.
    main_args.out_dir = os.path.join(
        main_args.out_dir, (main_args.target_os + "/" + main_args.target_cpu)
    )
    main_args.out_dir = os.path.abspath(main_args.out_dir)

    all_args = merge(all_args, main_args)


def create_out_dir(all_args: AllArgumentInfo) -> None:
    if not os.path.exists(all_args.out_dir):
        os.makedirs(all_args.out_dir)


def get_generator(main_args: AllArgumentInfo) -> str:
    """Determines the generator based on the platform."""

    # If we are on Windows, use Visual Studio
    if sys.platform == "win32":
        generator = "vs"
    # If we are on Mac, use XCode
    elif sys.platform == "darwin":
        generator = "xcode"
    # Otherwise, use the default
    else:
        generator = ""

    # If we are not generating, do not specify a generator
    if main_args.build_command != "gen":
        generator = ""

    return generator


def write_gn_args(
    all_args: AllArgumentInfo, project_configs: List[str]
) -> None:
    """Writes GN arguments to a file named 'args.gn'.

    This 'args.gn' file would be in the 'out/' folder.
    """

    args_gn = os.path.join(os.getcwd(), all_args.out_dir, "args.gn")
    with open(args_gn, "w", encoding="utf-8") as f:
        # os and cpu and is_debug
        f.write(f'target_os = "{all_args.target_os}"\n')
        f.write(f'target_cpu = "{all_args.target_cpu}"\n')
        f.write(
            "is_debug = {}\n".format(
                "true" if all_args.build_type == "debug" else "false"
            )
        )

        for arg in project_configs:
            f.write(arg + "\n")

        # Write the 'extra_args' specified in the command line to 'args.gn', so
        # that gn can recognize them.
        for arg in all_args.extra_args_list:
            f.write(str(arg).replace("#", '"') + "\n")


def dump_gn_args(all_args: AllArgumentInfo) -> None:
    tgn_args_file = os.path.join(os.getcwd(), all_args.out_dir, "tgn_args.txt")

    cmd = "{0} args --list {1} --short".format(
        all_args.gn_path,
        all_args.out_dir,
    )
    run_and_redirect_output(cmd, tgn_args_file)


def prepare_gn_args(all_args: AllArgumentInfo) -> None:
    """Prepares GN arguments by reading a PROJECTCONFIG.gn file."""

    # Read the PROJECTCONFIG.gn file and parse out the project configs.
    project_configs = []
    if os.path.exists("PROJECTCONFIG.gn"):
        with open("PROJECTCONFIG.gn", "r", encoding="utf-8") as f:
            for line in f.readlines():
                project_configs.append(line.strip())

    # Write the project configs to the GN args file.
    write_gn_args(all_args, project_configs)


def prepare_gn_files(all_args: AllArgumentInfo) -> None:
    """Prepares GN files by creating symbolic links."""

    dot_file: str = os.path.abspath(".gn")
    build_file: str = os.path.abspath(".gnfiles")
    default_dot_file: str = os.path.abspath(
        os.path.join(all_args.script_path, ".gn")
    )
    default_build_file: str = os.path.abspath(all_args.script_path)

    # If the symlink already exists, there's nothing to do.
    if (
        os.path.exists(dot_file)
        and not os.path.islink(dot_file)
        and os.path.exists(build_file)
    ):
        return

    if sys.platform == "win32":
        # On Windows, we can't just use ln, so we need to use mklink for both
        # of these.
        run_cmd("del -f .gn", False)
        run_cmd("rmdir /s /q .gnfiles", False)
        run_cmd(f"mklink .gn {default_dot_file}", False)
        run_cmd(f"mklink /d .gnfiles {default_build_file}", False)
    else:
        # On other platforms, we can just use ln.
        run_cmd(f"rm -f .gn; ln -s {default_dot_file} .gn", False)
        run_cmd(
            f"rm -f .gnfiles; ln -s {default_build_file} .gnfiles",
            False,
        )


def filter_target(all_args: AllArgumentInfo) -> str:
    """Get the label of the specified build target from 'gn ls' command.

    It first runs the 'gn gen' command to generate build files. Then, it lists
    all defined targets using the 'gn ls' command. It loops through each label
    and checks if it matches the desired build target. If a match is found, it
    returns the label. Otherwise, it returns an empty string.
    """

    if not all_args.build_target:
        return ""

    # 'gen' first.
    cmd = "{} gen {} --root={}".format(
        all_args.gn_path,
        all_args.out_dir,
        os.path.abspath(".").replace("\\", "/"),
    )
    run_cmd(cmd, echo=all_args.verbose)

    # Then, list all defined targets.
    cmd = "{} ls {} --root={}".format(
        all_args.gn_path,
        all_args.out_dir,
        os.path.abspath(".").replace("\\", "/"),
    )
    _, labels = get_cmd_output(cmd, echo=all_args.verbose)

    separator = all_args.build_target.find(":")
    if separator != -1:
        desired_label_name = all_args.build_target[(separator + 1) :]  # noqa
        desired_label_dir = all_args.build_target[0:separator]
    else:
        desired_label_name = all_args.build_target
        desired_label_dir = ""

    for label in labels.split("\n"):
        if str(label).find(":") == -1:
            # Every gn label should contain a ":", so I wonder if this case
            # could happen.
            continue

        label_dir, label_name = label.split(":")

        if desired_label_dir and label_dir != desired_label_dir:
            continue

        if label_name == desired_label_name:
            return label

    print(f"\nUnknown target '{all_args.build_target}'\n")
    sys.exit(-1)


def generate_solution(all_args: AllArgumentInfo) -> None:
    """This function prepares GN (Generate Ninja) arguments and files.

    It determines the generator to use (e.g., Xcode, Visual Studio) and sets
    the appropriate flags. It runs the 'gn gen' command to generate the build
    files. If a compile_commands.json file is generated, it copies this file to
    the project root directory. So that many other tools could find this file.
    """

    prepare_gn_args(all_args)
    prepare_gn_files(all_args)
    all_args.generator = get_generator(all_args)

    ide_arg = (
        f"--ide={all_args.generator}" if len(all_args.generator) != 0 else ""
    )
    ide_flag = ""
    generator_target = ""

    if all_args.build_target:
        generator_target = '--filters="' + filter_target(all_args) + '"'

    if str(all_args.generator).startswith("xcode"):
        ide_flag = "--xcode-build-system=new"

    cmd = (
        "{0} gen {1} --root={2} {3} --workspace-path={4}"
        " --export-compile-commands {5} {6}".format(
            all_args.gn_path,
            all_args.out_dir,
            os.path.abspath(".").replace("\\", "/"),
            ide_arg,
            os.getcwd(),
            ide_flag,
            generator_target,
        )
    )
    run_or_die(cmd, echo=all_args.verbose)

    # Copy the generated compile_commands.json to the project root folder, so
    # that some other tools can find it.
    if os.path.exists(os.path.join(all_args.out_dir, "compile_commands.json")):
        shutil.copy(
            os.path.join(all_args.out_dir, "compile_commands.json"),
            os.path.join(os.getcwd(), "compile_commands.json"),
        )

    dump_gn_args(all_args)


def generate_dep_graph(all_args: AllArgumentInfo):
    """Use the 'gn gen' command with the --dot flag to generate a dependency
    graph between projects.
    """

    prepare_gn_args(all_args)
    prepare_gn_files(all_args)

    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    cmd = (
        f"{all_args.ninja_path} {keeprsp_flag} -C {all_args.out_dir} -t graph"
        f" {all_args.build_target} >/tmp/ag_graph.dot"
    )
    run_or_die(cmd, echo=all_args.verbose)

    cmd = "dot -Tsvg /tmp/ag_graph.dot -o {}".format(
        os.path.join(all_args.out_dir, "ag_graph.svg")
    )
    run_or_die(cmd, echo=all_args.verbose)


def build_solution(all_args: AllArgumentInfo) -> None:
    """This function builds the solution using the Ninja build system. It
    constructs the Ninja command with the appropriate arguments and then
    executes it.
    """

    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    cmd: str = (
        f"{all_args.ninja_path} {keeprsp_flag} -C"
        f" {all_args.out_dir} {'-v' if all_args.verbose else ''}"
        f" {all_args.build_target}"
    )
    run_or_die(cmd, echo=all_args.verbose)


def explain_build_solution(all_args: AllArgumentInfo) -> None:
    """This function runs the Ninja build system with the -d explain flag to
    provide an explanation of the build process."""

    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    run_or_die(
        (
            f"{all_args.ninja_path} {keeprsp_flag} -C"
            f" {all_args.out_dir} {all_args.build_target} -d explain"
        ),
        echo=all_args.verbose,
    )


def desc_solution(all_args: AllArgumentInfo) -> None:
    cmd = "{0} desc {1} --blame --tree --root={2} {3} >{4}".format(
        all_args.gn_path,
        all_args.out_dir,
        os.path.abspath(".").replace("\\", "/"),
        all_args.build_target,
        os.path.join(
            all_args.out_dir,
            "ag_desc.txt" if not all_args.out_file else all_args.out_file,
        ),
    )
    run_or_die(cmd, echo=all_args.verbose)


def show_path(all_args: AllArgumentInfo) -> None:
    separator = all_args.build_target.find("=")
    if separator != -1:
        src_label = all_args.build_target[0:separator]
        dest_label = all_args.build_target[(separator + 1) :]  # noqa
    else:
        raise ValueError(
            "'path' command needs a source label and a destination label."
        )

    cmd = "{0} path --all {1} {2} {3}".format(
        all_args.gn_path,
        all_args.out_dir,
        src_label,
        dest_label,
    )
    run_or_die(cmd, echo=all_args.verbose)


def show_refs(all_args: AllArgumentInfo) -> None:
    cmd = "{0} refs --tree {1} {2}".format(
        all_args.gn_path,
        all_args.out_dir,
        all_args.build_target,
    )
    run_or_die(cmd, echo=all_args.verbose)


def show_args(all_args: AllArgumentInfo) -> None:
    cmd = "{0} args --list {1}".format(
        all_args.gn_path,
        all_args.out_dir,
    )
    run_or_die(cmd, echo=all_args.verbose)


def check_solution(all_args: AllArgumentInfo) -> None:
    cmd = "{0} check {1}".format(
        all_args.gn_path,
        all_args.out_dir,
    )
    run_or_die(cmd, echo=all_args.verbose)


def show_input_output_solution(all_args: AllArgumentInfo) -> None:
    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    run_or_die(
        (
            f"{all_args.ninja_path} {keeprsp_flag} -C"
            f" {all_args.out_dir} -t query {all_args.build_target}"
        ),
        echo=all_args.verbose,
    )


def show_input_solution(all_args: AllArgumentInfo) -> None:
    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    run_or_die(
        (
            f"{all_args.ninja_path} {keeprsp_flag} -C"
            f" {all_args.out_dir} -t inputs {all_args.build_target}"
        ),
        echo=all_args.verbose,
    )


def show_deps_solution(all_args: AllArgumentInfo) -> None:
    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    run_or_die(
        (
            f"{all_args.ninja_path} {keeprsp_flag} -C"
            f" {all_args.out_dir} -t deps {all_args.build_target}"
        ),
        echo=all_args.verbose,
    )


def clean_solution(all_args: AllArgumentInfo) -> None:
    """This function cleans the build solution using the Ninja build system."""

    keeprsp_flag = "-d keeprsp" if all_args.build_type == "debug" else ""

    run_not_die(
        (
            f"{all_args.ninja_path} {keeprsp_flag}"
            " -C {all_args.out_dir} -t clean"
            f" {all_args.build_target}"
        ),
        echo=all_args.verbose,
    )


def uninstall_solution() -> None:
    """This function removes the .gn and .gnfiles directories from the project
    root.
    """

    if os.path.exists(".gn"):
        if sys.platform == "win32":
            run_cmd("del /f .gn", False)
        else:
            run_cmd("rm -rf .gn", False)
    if os.path.exists(".gnfiles"):
        if sys.platform == "win32":
            run_cmd("rmdir /s /q .gnfiles", False)
        else:
            run_cmd("rm -rf .gnfiles", False)


def setup_pythonpath() -> None:
    """Customize PYTHONPATH

    When using tg in a project, the project may have its own Python scripts.
    We can customize the python paths through the .tgnconfig.json file.
    """

    cwd = os.getcwd()
    config_file_path = os.path.join(cwd, ".tgnconfig.json")
    if os.path.exists(config_file_path):
        with open(config_file_path, "r", encoding="utf-8") as config_file:
            config = json.load(config_file)

            extra_pythonpath = config.get("extra_pythonpath", [])
            if extra_pythonpath:
                extra_pythonpath = [
                    os.path.join(cwd, path) for path in extra_pythonpath
                ]

                current_pythonpath = os.environ.get("PYTHONPATH", "")

                new_pythonpath = (
                    os.pathsep.join([current_pythonpath] + extra_pythonpath)
                    if current_pythonpath
                    else os.pathsep.join(extra_pythonpath)
                )

                os.environ["PYTHONPATH"] = new_pythonpath


def setup_env() -> None:
    setup_pythonpath()
    os.environ["NINJA_STATUS"] = "[%f/%t](%r) "


def main(argv: list[str]) -> int:
    """This is the main function that orchestrates the entire script.

    = It first checks the Python version.
    = It parses the command-line arguments to determine the desired action
      (e.g., generate dependency graph, build, clean).
    = Depending on the provided command, it calls the appropriate function to
    perform the desired action.
    """

    check_python_version()
    setup_env()

    all_args = create_all_args()
    determine_essential_paths(all_args)

    # If there is a '--' in the command line, the part preceding it is
    # considered as 'main_args', while the part following it is considered as
    # 'extra_args'.
    if "--" in argv:
        main_args_list = argv[: argv.index("--")]
        extra_args_list = argv[argv.index("--") + 1 :]  # noqa
    else:
        main_args_list = argv
        extra_args_list = []

    parse_main_args(all_args, main_args_list)
    all_args.extra_args_list = extra_args_list

    create_out_dir(all_args)

    if all_args.build_command == "graph":
        generate_dep_graph(all_args)
    elif all_args.build_command == "gen":
        generate_solution(all_args)
    elif all_args.build_command == "build":
        # Check if the project has already been generated. If not, generate the
        # project first.
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        build_solution(all_args)
    elif all_args.build_command == "rebuild":
        # Cleanup the build results, and build again.
        generate_solution(all_args)
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if os.path.exists(gn_file):
            clean_solution(all_args)
        build_solution(all_args)
    elif all_args.build_command == "clean":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        clean_solution(all_args)
    elif all_args.build_command == "uninstall":
        uninstall_solution()
    elif all_args.build_command == "explain_build":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        explain_build_solution(all_args)
    elif all_args.build_command == "show_input_output":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_input_output_solution(all_args)
    elif all_args.build_command == "show_input":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_input_solution(all_args)
    elif all_args.build_command == "show_deps":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_deps_solution(all_args)
    elif all_args.build_command == "desc":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        desc_solution(all_args)
    elif all_args.build_command == "path":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_path(all_args)
    elif all_args.build_command == "refs":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_refs(all_args)
    elif all_args.build_command == "args":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        show_args(all_args)
    elif all_args.build_command == "check":
        gn_file = os.path.join(".", all_args.out_dir, "build.ninja")
        if not os.path.exists(gn_file):
            generate_solution(all_args)
        check_solution(all_args)
    else:
        pass
    return 0


if __name__ == "__main__":
    main(sys.argv[1:])
