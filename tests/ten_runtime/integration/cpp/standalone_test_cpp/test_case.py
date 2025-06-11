"""
Test standalone_test_cpp.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import build_config, fs_utils


def test_standalone_test_cpp():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    extension_root_path = os.path.join(base_path, "ext")
    dot_ten_path = os.path.join(extension_root_path, ".ten")
    fs_utils.remove_tree(dot_ten_path)

    # Step 1:
    #
    # Standalone testing involves the use of ten_runtime, so use tman install to
    # install the ten_runtime system package.
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

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    # Step 2:
    #
    # Execute tgn gen to generate the build files.
    #
    # Regardless of whether it is a standalone or non-standalone build, the C++
    # compilation in the TEN world is unified and always starts from the app
    # folder. Therefore, we need to switch to the `.ten/app/` directory before
    # proceeding with the C++ `tgn` compilation.
    ten_app_path = os.path.join(extension_root_path, ".ten", "app")
    if not os.path.isdir(ten_app_path):
        assert False, f"The directory does not exist.: {ten_app_path}"

    tgn_gen_cmd = [
        "tgn",
        "gen",
        build_config_args.target_os,
        build_config_args.target_cpu,
        build_config_args.target_build,
        "--",
        "ten_enable_standalone_test=true",
    ]

    if sys.platform == "win32":
        if build_config_args.vs_version:
            tgn_gen_cmd.append(f"vs_version={build_config_args.vs_version}")
        tgn_gen_cmd = ["cmd", "/c"] + tgn_gen_cmd

    tgn_gen_process = subprocess.Popen(
        tgn_gen_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=ten_app_path,
    )
    tgn_gen_rc = tgn_gen_process.wait()
    assert tgn_gen_rc == 0

    # Step 3:
    #
    # Execute tgn build to build the extension and its test cases.
    tgn_build_cmd = [
        "tgn",
        "build",
        build_config_args.target_os,
        build_config_args.target_cpu,
        build_config_args.target_build,
    ]

    if sys.platform == "win32":
        tgn_build_cmd = ["cmd", "/c"] + tgn_build_cmd

    tgn_build_process = subprocess.Popen(
        tgn_build_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=ten_app_path,
    )
    tgn_build_rc = tgn_build_process.wait()
    assert tgn_build_rc == 0

    # Step 4:
    #
    # Run standalone test cases.
    tester_cmd = [
        os.path.join(
            extension_root_path,
            (
                "bin/ext_test"
                + (".exe" if build_config_args.target_os == "win" else "")
            ),
        ),
    ]

    my_env["TEN_ENABLE_MEMORY_TRACKING"] = "true" + ";" + my_env["PATH"]

    if build_config_args.target_os == "win":
        my_env["PATH"] = (
            os.path.join(
                extension_root_path,
                (".ten/app/ten_packages/system/ten_runtime/lib"),
            )
            + ";"
            + my_env["PATH"]
        )

    tester_process = subprocess.Popen(
        tester_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        # Only C++ compilation needs to be performed within the app folder; for
        # running tests, it switches back to the extension folder.
        cwd=extension_root_path,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
