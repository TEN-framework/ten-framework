"""
Test standalone_test_async_tmpl_async_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, fs_utils


def test_standalone_test_async_tmpl_async_python():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_name = "example_async_extension_python"

    extension_root_path = os.path.join(base_path, extension_name)
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create example_async_extension_python package directly.
    tman_create_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "create",
        "extension",
        extension_name,
        "--template",
        "default_async_extension_python",
        "--template-data",
        "class_name_prefix=Example",
    ]

    tman_create_process = subprocess.Popen(
        tman_create_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=base_path,
    )
    tman_create_process.wait()
    return_code = tman_create_process.returncode
    if return_code != 0:
        assert False, "Failed to create package."

    # Step 2:
    #
    # Install all the dependencies.
    tman_install_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "install",
        "--standalone",
    ]

    tman_install_process = subprocess.Popen(
        tman_install_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tman_install_process.wait()
    return_code = tman_install_process.returncode
    if return_code != 0:
        assert False, "Failed to install package."

    # Step 3:
    #
    # pip install the package.

    # Create virtual environment.
    venv_dir = os.path.join(extension_root_path, "venv")
    subprocess.run([sys.executable, "-m", "venv", venv_dir], check=True)

    # Launch virtual environment.
    my_env["VIRTUAL_ENV"] = venv_dir
    if sys.platform == "win32":
        venv_bin_dir = os.path.join(venv_dir, "Scripts")
    else:
        venv_bin_dir = os.path.join(venv_dir, "bin")
    my_env["PATH"] = venv_bin_dir + os.pathsep + my_env["PATH"]

    # Run bootstrap script based on platform
    if sys.platform == "win32":
        # On Windows, use Python bootstrap script directly
        print("Running bootstrap script on Windows...")
        bootstrap_script = os.path.join(
            extension_root_path, "tests/bin/bootstrap.py"
        )
        bootstrap_process = subprocess.Popen(
            [sys.executable, bootstrap_script],
            stdout=stdout,
            stderr=subprocess.STDOUT,
            env=my_env,
            cwd=extension_root_path,
        )
    else:
        # On Unix-like systems, use bash bootstrap script
        bootstrap_cmd = os.path.join(extension_root_path, "tests/bin/bootstrap")
        bootstrap_process = subprocess.Popen(
            bootstrap_cmd, stdout=stdout, stderr=subprocess.STDOUT, env=my_env
        )

    bootstrap_process.wait()
    if bootstrap_process.returncode != 0:
        assert False, "Failed to run bootstrap script."

    # Step 4:
    #
    # Set the required environment variables for the test.

    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    # my_env["ASAN_OPTIONS"] = "detect_leaks=0"

    if sys.platform == "linux":
        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                extension_root_path,
                (".ten/app/ten_packages/system/ten_runtime/lib/libasan.so"),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path
    elif sys.platform == "darwin":
        build_config_args = build_config.parse_build_config(
            os.path.join(root_dir, "tgn_args.txt"),
        )

        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    ".ten/app/ten_packages/system/ten_runtime/lib/"
                    "libclang_rt.asan_osx_dynamic.dylib"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["DYLD_INSERT_LIBRARIES"] = libasan_path

    # Step 5:
    #
    # Run the test.
    if sys.platform == "win32":
        start_script = os.path.join(extension_root_path, "tests/bin/start.py")
        tester_process = subprocess.Popen(
            [sys.executable, start_script, "-s"],
            stdout=stdout,
            stderr=subprocess.STDOUT,
            env=my_env,
            cwd=extension_root_path,
        )
    else:
        # On Unix-like systems, use bash start script
        test_cmd = [
            "tests/bin/start",
            "-s",
        ]
        tester_process = subprocess.Popen(
            test_cmd,
            stdout=stdout,
            stderr=subprocess.STDOUT,
            env=my_env,
            cwd=extension_root_path,
        )

    tester_rc = tester_process.wait()
    assert tester_rc == 0
