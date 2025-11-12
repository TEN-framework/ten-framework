# This script is an example of how to compile Cython code.
# Developers can put this script into the directory where the Cython files are
# located.
#
# The usage of this script is as follows:
#
# step0. put this script file to the directory where the Cython files are
#        located. If the file is in the root of the extension directory, then
#        all .pyx files will be compiled recursively.
# step1. replace the 'source_file_list' with the actual source files.
# step2. run the following command:
#
#        python3 cython_compiler.py -r <compile_root_dir>
#
# step3. the compiled dynamic library will be generated in <compile_root_dir>.

import argparse
import os
import glob
from setuptools import Extension, setup


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.compile_root_dir: str


def install_cython_if_needed():
    try:
        from Cython.Build import cythonize

        return True
    except ImportError:
        import subprocess
        import sys

        rc = subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "Cython"]
        )
        return rc == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Cython compiler")
    parser.add_argument(
        "-r",
        "--compile-root-dir",
        type=str,
        required=False,
        default=os.path.dirname(os.path.abspath(__file__)),
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    install_rc = install_cython_if_needed()
    if not install_rc:
        print("Failed to install Cython")
        exit(1)

    from Cython.Build import cythonize

    # Get the directory name of the directory.
    dir_name = os.path.basename(args.compile_root_dir)

    # Recursively find all .pyx files in the current directory and its
    # subdirectories.
    pyx_files = glob.glob(
        os.path.join(args.compile_root_dir, "**", "*.pyx"), recursive=True
    )

    if not pyx_files:
        print("No .pyx files found.")
        exit(0)

    extensions = []
    for pyx_file in pyx_files:
        # Calculate the relative module name by removing the base directory and
        # extension.
        rel_path = os.path.relpath(pyx_file, args.compile_root_dir)
        module_name = rel_path.replace(os.path.sep, ".").rsplit(".", 1)[0]

        # Create an Extension object for each .pyx file.
        extensions.append(Extension("*", [pyx_file]))

    # Compile the found .pyx files, keeping the original directory structure.
    setup(
        ext_modules=cythonize(extensions),
        script_args=["build_ext", "--inplace"],
        # The package_dir parameter is used to where the packed package will be
        # placed. In this case, the package will be placed in the current
        # directory.
        package_dir={dir_name: args.compile_root_dir},
    )
