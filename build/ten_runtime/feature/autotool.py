#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import os
import ast
import sys
import glob
from build.scripts import cmd_exec, fs_utils


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.project_path: str
        self.build_path: str
        self.install_path: str
        self.use_clang: bool
        self.c_standard: str
        self.cxx_standard: str
        self.preactions: list[str]
        self.log_level: int
        self.configure_cmd_line_options: list[str]
        self.configure_env_vars: list[str]
        self.system_dep_pkgs: list[str]


class AutotoolProject:
    def __init__(self, args: ArgumentInfo) -> None:
        self.args = args
        self.configure_cmd_line_options = []
        self.configure_env_vars = []

    def _fill_configure_attributes(self):
        for configure_cmd_line_option in self.args.configure_cmd_line_options:
            self.configure_cmd_line_options.extend(
                [configure_cmd_line_option.strip('"')]
            )

        for configure_env_var in self.args.configure_env_vars:
            self.configure_env_vars.extend([configure_env_var.strip('"')])

        if self.args.install_path:
            self.configure_cmd_line_options.extend(
                [f"--prefix={self.args.install_path}"]
            )

        # Compiler choice
        if self.args.use_clang:
            self.configure_env_vars.extend(["CC=clang", "CXX=clang++"])
            self.configure_cmd_line_options.extend(
                ["--cc=clang", "--cxx=clang++"]
            )
        else:
            if self.args.log_level:
                print("compiler choice: gcc\n")
            self.configure_env_vars.extend(["CC=gcc", "CXX=g++"])
            self.configure_cmd_line_options.extend(["--cc=gcc", "--cxx=g++"])

        self.configure_env_vars.extend(
            [
                f'CFLAGS="-std=c{self.args.c_standard}"',
                f'CXXFLAGS="-std=c++{self.args.cxx_standard}"',
            ]
        )

    def preaction(self):
        os.chdir(self.args.project_path)

        for preaction in self.args.preactions:
            cmd = f"{preaction}"

            if self.args.log_level:
                print(cmd)

            cmd_exec.run_cmd_realtime(cmd, log_level=self.args.log_level)

    def configure(self):
        self._fill_configure_attributes()

        if self.args.build_path != "":
            os.chdir(self.args.build_path)
        else:
            os.chdir(self.args.project_path)

        configure_path = os.path.join(self.args.project_path, "configure")
        cmd = (
            " ".join(self.configure_env_vars)
            + " "
            + configure_path
            + " "
            + " ".join(self.configure_cmd_line_options)
        )

        if self.args.log_level:
            print("config: \n" + cmd)

        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode:
            print("Failed to execute configure command.\n")
            print(output_text)
            sys.exit(returncode)

    def build(self):
        if self.args.build_path != "":
            os.chdir(self.args.build_path)
        else:
            os.chdir(self.args.project_path)

        cmd = f"make -j{os.cpu_count()}"

        if self.args.log_level:
            print("build: \n" + cmd)

        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode:
            print("Failed to execute build command.\n")
            print(output_text)
            sys.exit(returncode)

    def install(self):
        if self.args.build_path != "":
            os.chdir(self.args.build_path)
        else:
            os.chdir(self.args.project_path)

        cmd = "make install"

        if self.args.log_level:
            print("install: \n" + cmd)

        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode:
            print("Failed to execute install command.\n")
            print(output_text)
            sys.exit(returncode)

    def copy_system_deps(self):
        for dep in self.args.system_dep_pkgs:
            libs = []

            if sys.platform == "linux":
                libs.extend(
                    glob.glob(
                        f"/usr/lib/{os.uname().machine}-linux-gnu/lib{dep}.so.*"
                    )
                )
                libs.extend(
                    glob.glob(
                        f"/usr/lib/{os.uname().machine}-linux-gnu/lib{dep}.so"
                    )
                )
            else:
                print("TODO: Add support for other platforms.")
                sys.exit(1)

            for f in libs:
                dest = os.path.join(
                    self.args.install_path, "lib", os.path.basename(f)
                )
                fs_utils.copy_file(f, dest, False)

    def run(self):
        self.preaction()
        self.configure()
        self.build()
        self.install()
        self.copy_system_deps()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-path", type=str, required=True)
    parser.add_argument("--build-path", type=str, default="", required=False)
    parser.add_argument("--install-path", type=str, default="", required=False)
    parser.add_argument("--preactions", type=str, action="append", default=[])
    parser.add_argument(
        "--configure-cmd-line-options", type=str, action="append", default=[]
    )
    parser.add_argument(
        "--configure-env-vars", type=str, action="append", default=[]
    )
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )
    parser.add_argument("--use-clang", type=ast.literal_eval, default=True)
    parser.add_argument("--c-standard", type=str, help="specify c_standard")
    parser.add_argument(
        "--cxx-standard", type=str, help="specify cxx_standard or not"
    )
    parser.add_argument(
        "--system-dep-pkgs", type=str, action="append", default=[]
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    p = AutotoolProject(args)
    p.run()
