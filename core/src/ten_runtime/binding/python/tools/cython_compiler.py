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
import platform
import tempfile
import shutil
from setuptools import Extension, setup


class ArgumentInfo(argparse.Namespace):
    def __init__(self):
        super().__init__()

        self.compile_root_dir: str
        self.use_mingw: bool


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
    parser.add_argument(
        "--use-mingw",
        action="store_true",
        help="Force using MinGW compiler on Windows",
    )

    arg_info = ArgumentInfo()
    args = parser.parse_args(namespace=arg_info)

    # Detect compiler on Windows
    if platform.system() == "Windows":
        gcc_path = shutil.which("gcc")

        if args.use_mingw:
            if gcc_path:
                print(f"Using MinGW gcc at: {gcc_path} to compile cython")
                os.environ["CC"] = "gcc"
                os.environ["CXX"] = "g++"
            else:
                print("ERROR: --use-mingw specified but gcc not found in PATH")
                exit(1)
        # Otherwise, auto-detect but warn if mismatch is possible
        elif gcc_path:
            print("WARNING: gcc found in PATH but --use-mingw not specified")
            print("  If your project is built with MinGW,")
            print("  please add --use-mingw flag to ensure ABI compatibility")
            print("  Will use MSVC by default")
        else:
            print("Using MSVC compiler (default on Windows)")

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

    extensions = []
    for pyx_file in pyx_files:
        # Calculate the relative module name by removing the base directory and
        # extension.
        rel_path = os.path.relpath(pyx_file, args.compile_root_dir)
        module_name = rel_path.replace(os.path.sep, ".").rsplit(".", 1)[0]

        # Create an Extension object for each .pyx file.
        # On Windows with MinGW, we need to fix SIZEOF_VOID_P mismatch
        # Python's pyconfig.h checks for MS_WIN64 (MSVC-specific), but MinGW only defines _WIN64
        extra_compile_args = []
        if platform.system() == "Windows" and os.environ.get("CC") == "gcc":
            import struct
            pointer_size = struct.calcsize("P")
            if pointer_size == 8:
                # Define MS_WIN64 for every extension so pyconfig.h uses the correct
                # SIZEOF_VOID_P=8 (Or else it will be 4)
                extra_compile_args.append("-DMS_WIN64")

        extensions.append(Extension("*", [pyx_file], extra_compile_args=extra_compile_args))

    # Compile the found .pyx files, keeping the original directory structure.
    # Prepare script arguments for setup
    script_args = ["build_ext", "--inplace"]

    # On Windows with MSVC, use a short temp directory for build files to avoid
    # path length issues. MSVC has a ~260 character path limit, and setuptools
    # creates deep directory structures that mirror the source file's absolute
    # path, which can easily exceed this limit.
    #
    # Strategy: Copy .pyx files to a short temp directory, compile there,
    # then copy the resulting .pyd files back to the original location.
    temp_dir = None
    original_pyx_locations = {}  # Map from temp .pyx path to original path

    if platform.system() == "Windows" and not args.use_mingw:
        temp_dir = tempfile.mkdtemp(prefix="cython_")
        script_args.append(f"--build-temp={temp_dir}")
        print(f"Using temporary directory: {temp_dir}")

        # Copy .pyx files to temp directory, preserving relative structure
        # Must preserve the "dir_name" in the temp directory structure because
        # Cython derives the module name from the file path.
        new_extensions = []
        for pyx_file in pyx_files:
            rel_path = os.path.relpath(pyx_file, args.compile_root_dir)
            temp_pyx_path = os.path.join(temp_dir, dir_name, rel_path)
            os.makedirs(os.path.dirname(temp_pyx_path), exist_ok=True)
            shutil.copy2(pyx_file, temp_pyx_path)
            original_pyx_locations[temp_pyx_path] = pyx_file
            print(f"[DEBUG] {rel_path} -> {temp_pyx_path}")
            new_extensions.append(Extension("*", [temp_pyx_path]))

        extensions = new_extensions

    # On Windows with MinGW, explicitly specify the compiler
    if platform.system() == "Windows" and args.use_mingw:
        script_args.append("--compiler=mingw32")

        # Fix for MinGW: .def files are not compatible with MinGW's ld
        # Cython already uses __declspec(dllexport) for symbol export,
        # so .def files are redundant and cause linking errors with MinGW
        # Ref:
        # https://setuptools.pypa.io/en/latest/deprecated/distutils/apiref.html#distutils.ccompiler.CCompiler.link
        try:
            # Try setuptools' vendored distutils first (Python 3.10+)
            from setuptools._distutils import cygwinccompiler
            print("[DEBUG] Using setuptools._distutils.cygwinccompiler")
        except ImportError:
            # Fall back to standard distutils
            from distutils import cygwinccompiler
            print("[DEBUG] Using distutils.cygwinccompiler")

        # Save the original link method
        _original_link = cygwinccompiler.Mingw32CCompiler.link

        def _patched_link(self, target_desc, objects, output_filename,
                         output_dir=None, libraries=None, library_dirs=None,
                         runtime_library_dirs=None, export_symbols=None,
                         debug=0, extra_preargs=None, extra_postargs=None,
                         build_temp=None, target_lang=None):

            print(f"[DEBUG] link() called for: {os.path.basename(output_filename)}")
            print(f"[DEBUG] export_symbols (before): {export_symbols}")

            # Force export_symbols to None to prevent .def file generation
            # MinGW will automatically export symbols marked with __declspec(dllexport)
            export_symbols = None

            print(f"[DEBUG] export_symbols (after): {export_symbols}")

            return _original_link(self, target_desc, objects, output_filename,
                                output_dir, libraries, library_dirs,
                                runtime_library_dirs, export_symbols,
                                debug, extra_preargs, extra_postargs,
                                build_temp, target_lang)

        # Monkey-patch the class method
        cygwinccompiler.Mingw32CCompiler.link = _patched_link
        print("[DEBUG] Successfully patched Mingw32CCompiler.link")

    package_dir = {
        dir_name: (os.path.join(temp_dir, dir_name)
        if temp_dir else args.compile_root_dir)
    }

    setup(
        ext_modules=cythonize(extensions),
        script_args=script_args,
        # The package_dir parameter is used to where the packed package will be
        # placed. In this case, the package will be placed in the current
        # directory.
        # On Windows with MSVC, the compiled .pyd files will be manually copy
        # back to original locations
        package_dir=package_dir,
    )

    if temp_dir:
        print("Copying compiled .pyd files back to original locations...")
        for temp_pyx_path, original_pyx_path in original_pyx_locations.items():
            temp_pyx_dir = os.path.dirname(temp_pyx_path)
            pyx_basename = os.path.splitext(os.path.basename(temp_pyx_path))[0]

            if os.path.exists(temp_pyx_dir):
                all_files = os.listdir(temp_pyx_dir)
                print(f"[DEBUG] files in {temp_pyx_dir}: {all_files}")
                for f in all_files:
                    if f.startswith(pyx_basename) and f.endswith('.pyd'):
                        src_pyd = os.path.join(temp_pyx_dir, f)
                        dst_pyd = os.path.join(os.path.dirname(original_pyx_path), f)
                        shutil.copy2(src_pyd, dst_pyd)
                        print(f"  Copied {f} to {os.path.dirname(original_pyx_path)}")

    # Clean up temporary directory
    if temp_dir and os.path.exists(temp_dir):
        shutil.rmtree(temp_dir, ignore_errors=True)
        print(f"Cleaned up temporary directory: {temp_dir}")
