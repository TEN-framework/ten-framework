"""
Test websocket_server_nodejs.
"""

import subprocess
import os
from sys import stdout
import sys
from .utils import fs_utils, build_config


def test_standalone_test_nodejs():
    """Test standalone_test_nodejs."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "default_extension_nodejs")
    fs_utils.remove_tree(extension_root_path)

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create default_extension_nodejs package directly.
    tman_create_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "create",
        "extension",
        "default_extension_nodejs",
        "--template",
        "default_extension_nodejs",
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
    # Install all the dependencies of the default_extension_nodejs package.
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

    # extension folder in app is
    # .ten/app/ten_packages/extension/default_extension_nodejs/ directory.
    extension_in_app_folder = os.path.join(
        extension_root_path,
        ".ten/app/ten_packages/extension/default_extension_nodejs",
    )
    assert os.path.exists(extension_in_app_folder)

    # Step 3:
    #
    # Standalone install and build the default_extension_nodejs package.
    standalone_install_cmd = [
        "npm",
        "run",
        "standalone-install",
    ]
    standalone_install_process = subprocess.Popen(
        standalone_install_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_in_app_folder,
    )
    standalone_install_process.wait()

    return_code = standalone_install_process.returncode
    if return_code != 0:
        assert False, "Failed to standalone install package."

    build_extension_cmd = [
        "npm",
        "run",
        "build",
    ]
    build_extension_process = subprocess.Popen(
        build_extension_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_in_app_folder,
    )
    build_extension_process.wait()

    return_code = build_extension_process.returncode
    if return_code != 0:
        assert False, "Failed to build package."

    # Step 4:
    #
    # Run the test.
    test_cmd = [
        "tests/bin/start",
    ]

    if sys.platform == "linux" and build_config_args.enable_sanitizer:
        test_cmd.append("-asan")

    tester_process = subprocess.Popen(
        test_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_in_app_folder,
    )
    tester_rc = tester_process.wait()
    assert tester_rc == 0
