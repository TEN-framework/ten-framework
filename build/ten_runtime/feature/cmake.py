#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import argparse
import time
import re
import os
import ast
from build.scripts import cmd_exec, timestamp_proxy
from ten_common.scripts import env


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.project_path: str
        self.project_name: str
        self.build_path: str
        self.build_type: str
        self.use_clang: bool
        self.hide_symbol: str
        self.target_os: str
        self.target_cpu: str
        self.log_level: int
        self.c_standard: str
        self.cxx_standard: str
        self.enable_c_extensions: str
        self.cflags: list[str]
        self.cxxflags: list[str]
        self.sharedlinkerflags: list[str]
        self.exelinkerflags: list[str]
        self.options: list[str]
        self.install_path: str
        self.run_build: str
        self.root_out_dir: str
        self.tg_timestamp_proxy_file: str


class CmakeProject:
    def __init__(self, args: ArgumentInfo):
        self.args = args
        self.env = []
        self.defines = []
        self.cflags = []
        self.cxxflags = []
        self.sharedlinkerflags = []
        self.exelinkerflags = []
        self.gen_cmds = []
        self.build_cmds = []
        self.cmake_version = ""

    # Cmake must be at least version 3.13.5 or above.
    def _version_check(self, cur_cmake_version: str):
        items = str(cur_cmake_version).split(".")
        if int(items[0]) > 3:
            return True
        elif int(items[0]) == 3:
            if int(items[1]) > 13:
                return True
            elif int(items[1] == 13):
                if int(items[2]) >= 5:
                    return True
        return False

    def _env_check(self):
        status, output = cmd_exec.get_cmd_output("cmake --version")
        if status != 0:
            raise RuntimeError("Failed to get Cmake version.")
        else:
            pattern = re.compile(r"cmake version (\d+.\d+.\d+)", re.DOTALL)
            items = pattern.findall(output)
            self.cmake_version = items[0]
            if not self._version_check(self.cmake_version):
                raise RuntimeError(
                    f"Your cmake version is {items[0]} less than 3.13.5"
                )
            else:
                return True

    def _get_cached_variable_value(self, var):
        if os.path.exists(f"{self.args.build_path}/CMakeCache.txt"):
            cmd = f"cmake -LA -N {self.args.build_path}"

            if self.args.log_level > 1:
                print(f"> {cmd}")

            returncode, output_text = cmd_exec.run_cmd_realtime(
                cmd, log_level=self.args.log_level
            )
            if returncode != 0:
                print(output_text)
                raise RuntimeError("Failed to get cached variable value.")

            # The regex pattern is to match {var}:xxx=yyy
            p = re.compile(f"{var}(?::[a-zA-Z0-9]*)?=(.*)")
            s = p.search(output_text)
            if s is not None:
                return s.group(1)
            else:
                return None
        else:
            return None

    def _is_cached_c_compiler_is_clang(self, var):
        if var.find("clang") != -1:
            return True
        else:
            return False

    def process_flags_from_arg(self, flags: list[str], args_flags: list[str]):
        if args_flags:
            for flag in args_flags:
                if (
                    isinstance(flag, str)
                    and flag.startswith('"')
                    and flag.endswith('"')
                ):
                    # Remove the first and last character from the string
                    new_flag = flag[1:-1]
                    flags.append(new_flag)
                else:
                    # Add unmodified item
                    raise ValueError(
                        "Enclose the value in double quotes, otherwise \
cmake.py will consider it as its own command line option."
                    )

    def _fill_attributes(self):
        self.defines.append(f"-DCMAKE_BUILD_TYPE={self.args.build_type}")

        if self.args.install_path:
            self.defines.append(
                f"-DCMAKE_INSTALL_PREFIX={self.args.install_path}"
            )

        # Compiler choice
        if self.args.target_os != "android":
            if self.args.use_clang:
                # If specifying different C/C++ compiler, we need to clean/
                # delete the CMake cache first. Please refer to:
                # https://stackoverflow.com/questions/48178264/cmake-cache-variables-deleted-when-some-change
                cached_c_compiler = self._get_cached_variable_value(
                    "CMAKE_C_COMPILER"
                )

                if (
                    cached_c_compiler is not None
                    and self._is_cached_c_compiler_is_clang(cached_c_compiler)
                    is False
                ):
                    print("Specify clang, but cached gcc, clean first.")
                    self.clean()

                self.defines.extend(
                    [
                        "-DCMAKE_C_COMPILER:FILEPATH=clang",
                        "-DCMAKE_CXX_COMPILER:FILEPATH=clang++",
                        "-DCMAKE_LINKER:FILEPATH=lld",
                    ]
                )
            else:
                cached_c_compiler = self._get_cached_variable_value(
                    "CMAKE_C_COMPILER"
                )

                if (
                    cached_c_compiler is not None
                    and self._is_cached_c_compiler_is_clang(cached_c_compiler)
                    is True
                ):
                    print("Specify gcc, but cached clang, clean first.")
                    self.clean()

                self.defines.extend(
                    [
                        "-DCMAKE_C_COMPILER:FILEPATH=gcc",
                        "-DCMAKE_CXX_COMPILER:FILEPATH=g++",
                    ]
                )

        if self.args.target_os == "linux":
            if self.args.target_cpu == "x86":
                self.cflags.append("-m32")
            elif self.args.target_cpu == "x64":
                self.cflags.append("-m64")
            elif self.args.target_cpu == "arm":
                self.cflags.append(
                    "-march=armv7-a -mfloat-abi=softfp"
                    " --target=arm-linux-gnueabihf -mfloat-abi=$arm_float_abi"
                )
                self.sharedlinkerflags.append("--target=arm-linux-gnueabihf")
                self.exelinkerflags.append("--target=arm-linux-gnueabihf")
            elif self.args.target_cpu == "arm64":
                if os.uname().machine in ["arm64", "aarch64"]:
                    pass
                else:
                    self.cflags.append("--target=aarch64-linux-gnu")
                    self.sharedlinkerflags.append("--target=aarch64-linux-gnu")
                    self.exelinkerflags.append("--target=aarch64-linux-gnu")
                    if self.args.use_clang:
                        self.cflags.append("-fuse-ld=lld")
                        self.sharedlinkerflags.append("-fuse-ld=lld")
                        self.exelinkerflags.append("-fuse-ld=lld")
            else:
                raise RuntimeError(
                    "Currently can not build Linux target with CPU arch"
                    f" ${self.args.target_cpu}"
                )

            sysroot = os.environ.get("AG_SYSROOT")
            if sysroot:
                self.defines.append(f"-DCMAKE_SYSROOT={sysroot}")
        elif self.args.target_os == "mac":
            if self.args.target_cpu == "x64":
                self.cflags.append("--target=x86_64-apple-darwin")
                self.defines.append("-DCMAKE_APPLE_SILICON_PROCESSOR=x86_64")
            elif self.args.target_cpu == "arm64":
                self.cflags.append("--target=arm64-apple-darwin")
                self.defines.append("-DCMAKE_APPLE_SILICON_PROCESSOR=arm64")

        # https://cmake.org/cmake/help/latest/generator/Visual%20Studio%2017%202022.html
        elif self.args.target_os == "win":
            if self.args.target_cpu == "x64":
                self.defines.append("-DCMAKE_GENERATOR_PLATFORM=x64")
            elif self.args.target_cpu == "x86":
                self.defines.append("-DCMAKE_GENERATOR_PLATFORM=Win32")
            else:
                self.defines.append("-DCMAKE_GENERATOR_PLATFORM=ARM64")

        self.process_flags_from_arg(self.cflags, self.args.cflags)
        self.process_flags_from_arg(self.cxxflags, self.args.cxxflags)
        self.process_flags_from_arg(
            self.sharedlinkerflags, self.args.sharedlinkerflags
        )
        self.process_flags_from_arg(
            self.exelinkerflags, self.args.exelinkerflags
        )

        for opt in self.args.options:
            if str(opt).startswith("CMAKE_C_FLAGS"):
                raise ValueError("Using cflags to specify this one.")
            elif str(opt).startswith("CMAKE_CXX_FLAGS"):
                raise ValueError("Using cxxflags to specify this one.")
            elif str(opt).startswith("CMAKE_SHARED_LINKER_FLAGS"):
                raise ValueError("Using sharedlinkerflags to specify this one.")
            elif str(opt).startswith("CMAKE_EXE_LINKER_FLAGS"):
                raise ValueError("Using exelinkerflags to specify this one.")
            elif str(opt).startswith("-D"):
                self.defines.append(opt)
            else:
                self.defines.append(f"-D{opt}")

        # CMake 3.5 was released in 2018, and newer versions of CMake no longer
        # support compatibility with versions earlier than 3.5. If
        # `CMakeLists.txt` contains a line like
        # `cmake_minimum_required(VERSION x.x)` and `x.x` is less than 3.5,
        # newer versions of CMake will throw an error. We don’t want to modify
        # this line across numerous third-party libraries, we use a trick here
        # by overriding it directly in the CMake command line.
        self.defines.append("-DCMAKE_POLICY_VERSION_MINIMUM=3.5")

        if self.args.target_os == "linux":
            self.sharedlinkerflags.extend(
                [
                    "-Wl,-rpath=\\$ORIGIN",
                    "-Wl,-rpath=\\$ORIGIN/../lib/",
                ]
            )

            if self.args.enable_c_extensions == "true":
                self.defines.append("-DCMAKE_C_EXTENSIONS=ON")
            else:
                self.defines.append("-DCMAKE_C_EXTENSIONS=OFF")
        elif self.args.target_os == "mac":
            self.sharedlinkerflags.extend(
                [
                    "-Wl,-rpath,@loader_path",
                    "-Wl,-rpath,@loader_path/../lib/",
                ]
            )
            if self.args.enable_c_extensions == "true":
                self.defines.append("-DCMAKE_C_EXTENSIONS=ON")
            else:
                self.defines.append("-DCMAKE_C_EXTENSIONS=OFF")

        if self.args.hide_symbol == "true":
            self.cflags.append("-fvisibility=hidden")

        if self.args.target_os != "win":
            self.defines.extend(
                [
                    f"-DCMAKE_C_STANDARD={self.args.c_standard}",
                    f"-DCMAKE_CXX_STANDARD={self.args.cxx_standard}",
                ]
            )

        if len(self.cflags) > 0:
            cflags_cmds = " ".join(self.cflags)
            self.defines.append("-DCMAKE_C_FLAGS='" + cflags_cmds + "'")

        if len(self.cflags) > 0 or len(self.cxxflags) > 0:
            cflags_cmds = " ".join(self.cflags)
            cxxflags_cmds = " ".join(self.cxxflags)
            self.defines.append(
                "-DCMAKE_CXX_FLAGS='" + cflags_cmds + " " + cxxflags_cmds + "'"
            )

        if len(self.sharedlinkerflags) > 0:
            ldflags_cmds = " ".join(self.sharedlinkerflags)
            self.defines.append(
                "-DCMAKE_SHARED_LINKER_FLAGS='" + ldflags_cmds + "'"
            )

        if len(self.exelinkerflags) > 0:
            exeflags_cmds = " ".join(self.exelinkerflags)
            self.defines.append(
                "-DCMAKE_EXE_LINKER_FLAGS='" + exeflags_cmds + "'"
            )

    def gen(self):
        self._fill_attributes()

        # From 'project_path' to 'build_path'
        cmd = (
            " ".join(self.env)
            + f" cmake -S {self.args.project_path} -B {self.args.build_path} "
            + " ".join(self.defines)
        )

        if self.args.log_level > 1:
            print(f"> {cmd}")

        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode != 0:
            # Something went wrongs, dump env variables to check.
            env.dump_env_vars()
            print(output_text)
            raise RuntimeError("Failed to cmake -S.")

    def clean(self):
        # It seems that 'cmake --target clean' does _not_ clean the CMake cache.
        # The only reliable way to clean CMake cache is to delete the build
        # folder directly.
        #
        # cmd = ' '.join(self.env) + f' cmake --build {self.args.build_path}
        # --target clean'
        cmd = f"rm -rf {self.args.build_path}"
        if self.args.log_level > 1:
            print(f"> {cmd}")
        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode != 0:
            print(output_text)
            raise RuntimeError("Failed to cmake clean.")

    def build(self):
        cmd = (
            " ".join(self.env)
            + f" cmake --build {self.args.build_path}"
            + f" --config {self.args.build_type}"
            + f" --target {self.args.project_name}"
        )

        if self.args.target_os != "win" and self.args.target_os != "mac":
            cmd += " --parallel $(nproc)"

        if self.args.log_level > 1:
            print(f"> {cmd}")
        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode != 0:
            print(output_text)
            raise RuntimeError("Failed to cmake build.")

    def install(self):
        cmd = (
            " ".join(self.env)
            + f" cmake --install {self.args.build_path} --config"
            f" {self.args.build_type}"
        )
        if self.args.log_level > 1:
            print(f"> {cmd}")
        returncode, output_text = cmd_exec.run_cmd_realtime(
            cmd, log_level=self.args.log_level
        )
        if returncode != 0:
            print(output_text)
            raise RuntimeError("Failed to cmake install.")

    def run(self):
        self._env_check()

        self.gen()

        if self.args.run_build == "true":
            # TODO(Jingui): If we don't sleep here, cmake build may be error:
            # 'could not load cache'
            time.sleep(5)

            self.build()

        if self.args.install_path:
            self.install()


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-path", type=str, required=True)
    parser.add_argument("--project-name", type=str, required=True)
    parser.add_argument("--build-path", type=str, required=True)
    parser.add_argument("--build-type", type=str, default="Release")
    parser.add_argument("--use-clang", type=ast.literal_eval, default=True)
    parser.add_argument(
        "--target-os", type=str, required=True, help="specify target os"
    )
    parser.add_argument(
        "--target-cpu", type=str, required=True, help="specify target cpu"
    )
    parser.add_argument(
        "--log-level", type=int, required=True, help="specify log level"
    )
    parser.add_argument("--c-standard", type=str, help="specify c_standard")
    parser.add_argument(
        "--cxx-standard", type=str, help="specify cxx_standard or not"
    )
    parser.add_argument(
        "--hide-symbol",
        type=str,
        default="false",
        help="specify hide_symbol or not",
    )
    parser.add_argument(
        "--enable-c-extensions",
        type=str,
        help="enable compiler C extensions or not",
    )
    parser.add_argument("--cflags", type=str, action="append", default=[])
    parser.add_argument("--cxxflags", type=str, action="append", default=[])
    parser.add_argument(
        "--sharedlinkerflags", type=str, action="append", default=[]
    )
    parser.add_argument(
        "--exelinkerflags", type=str, action="append", default=[]
    )
    parser.add_argument("--options", type=str, action="append", default=[])
    parser.add_argument("--install-path", type=str, default="")
    parser.add_argument("--run-build", type=str, default="true")
    parser.add_argument("--root-out-dir", type=str, required=True)
    parser.add_argument("--tg-timestamp-proxy-file", type=str, required=True)

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    p = CmakeProject(args)
    p.run()

    timestamp_proxy.touch_timestamp_proxy_file(args.tg_timestamp_proxy_file)
