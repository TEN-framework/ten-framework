"""
Test standalone_ollama_async_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, build_pkg, fs_utils


def test_standalone_ollama_async_python():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "ollama_python")
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create ollama_python package directly.
    tman_create_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "create",
        "extension",
        "ollama_python",
        "--template",
        "ollama_python",
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
    # Install all the dependencies of the ollama_python package.
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
    my_env["PATH"] = os.path.join(venv_dir, "bin") + os.pathsep + my_env["PATH"]

    bootstrap_cmd = os.path.join(extension_root_path, "tests/bin/bootstrap")

    bootstrap_process = subprocess.Popen(
        bootstrap_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    bootstrap_process.wait()

    # Step 4:
    #
    # Set the required environment variables for the test.

    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    # It looks like there are two memory leaks below that are unrelated to TEN.
    #
    # Direct leak of 160 byte(s) in 2 object(s) allocated from:
    # #0 in __interceptor_realloc .../libsanitizer/asan/asan_malloc_linux.cpp:164
    # #1 (.../ollama_python/venv/lib/python3.10/site-packages/pydantic_core/_pydantic_core.cpython-310-x86_64-linux-gnu.so+0x2bdacc)
    #
    # Direct leak of 160 byte(s) in 5 object(s) allocated from:
    # #0 in __interceptor_malloc .../src/libsanitizer/asan/asan_malloc_linux.cpp:145
    # #1 (.../ollama_python/venv/lib/python3.10/site-packages/pydantic_core/_pydantic_core.cpython-310-x86_64-linux-gnu.so+0x2bbfaa)
    #
    # SUMMARY: AddressSanitizer: 320 byte(s) leaked in 7 allocation(s).
    my_env["ASAN_OPTIONS"] = "detect_leaks=0"

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
    tester_process = subprocess.Popen(
        "tests/bin/start",
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
