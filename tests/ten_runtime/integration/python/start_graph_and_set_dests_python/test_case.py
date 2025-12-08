"""
Test start_graph_and_set_dests_python.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import msgpack, build_config, build_pkg, fs_utils


def test_start_graph_and_set_dests_python():
    """Test client and app server."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    # Create virtual environment.
    venv_dir = os.path.join(base_path, "venv")
    subprocess.run([sys.executable, "-m", "venv", venv_dir], check=True)

    my_env = os.environ.copy()

    # Set the required environment variables for the test.
    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    # Launch virtual environment.
    my_env["VIRTUAL_ENV"] = venv_dir
    if sys.platform == "win32":
        venv_bin_dir = os.path.join(venv_dir, "Scripts")
    else:
        venv_bin_dir = os.path.join(venv_dir, "bin")
    my_env["PATH"] = venv_bin_dir + os.pathsep + my_env["PATH"]

    app_dir_name = "start_graph_and_set_dests_python_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "python"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    if build_config_args.ten_enable_integration_tests_prebuilt is False:
        # Before starting, cleanup the old app package.
        fs_utils.remove_tree(app_root_path)

        print(f'Assembling and building package "{app_dir_name}".')

        rc = build_pkg.prepare_and_build_app(
            build_config_args,
            root_dir,
            base_path,
            app_dir_name,
            app_language,
        )
        if rc != 0:
            assert False, "Failed to build package."

    tman_install_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "install",
    ]

    tman_install_process = subprocess.Popen(
        tman_install_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )
    tman_install_process.wait()
    return_code = tman_install_process.returncode
    if return_code != 0:
        assert False, "Failed to install package."

    # Run bootstrap script based on platform
    if sys.platform == "win32":
        # On Windows, use Python bootstrap script directly
        print("Running bootstrap script on Windows...")
        bootstrap_script = os.path.join(app_root_path, "bin/bootstrap.py")
        bootstrap_process = subprocess.Popen(
            [sys.executable, bootstrap_script],
            stdout=stdout,
            stderr=subprocess.STDOUT,
            env=my_env,
            cwd=app_root_path,
        )
    else:
        # On Unix-like systems, use bash bootstrap script
        bootstrap_cmd = os.path.join(app_root_path, "bin/bootstrap")
        bootstrap_process = subprocess.Popen(
            bootstrap_cmd, stdout=stdout, stderr=subprocess.STDOUT, env=my_env
        )

    bootstrap_process.wait()
    if bootstrap_process.returncode != 0:
        assert False, "Failed to run bootstrap script."

    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "start_graph_and_set_dests_python_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    if sys.platform == "win32":
        start_script = os.path.join(app_root_path, "bin", "start.py")

        if not os.path.isfile(start_script):
            print(f"Server command '{start_script}' does not exist.")
            assert False

        server_cmd = [sys.executable, start_script]
    else:
        server_cmd = os.path.join(app_root_path, "bin/start")

        if not os.path.isfile(server_cmd):
            print(f"Server command '{server_cmd}' does not exist.")
            assert False

    if sys.platform == "win32":
        client_cmd = os.path.join(
            base_path, "start_graph_and_set_dests_python_app_client.exe"
        )
    else:
        client_cmd = os.path.join(
            base_path, "start_graph_and_set_dests_python_app_client"
        )

    server = subprocess.Popen(
        server_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=app_root_path,
    )

    is_started, sock = msgpack.is_app_started("127.0.0.1", 8001, 30)
    if not is_started:
        print(
            "The start_graph_and_set_dests_python is not started after 30 seconds."
        )

        server.kill()
        exit_code = server.wait()
        print(
            "The exit code of start_graph_and_set_dests_python: ",
            exit_code,
        )

        assert exit_code == 0
        assert False

    if sys.platform == "win32":
        # client depends on some libraries in the TEN app.
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "start_graph_and_set_dests_python_app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
    elif sys.platform == "darwin":
        # client depends on some libraries in the TEN app.
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "start_graph_and_set_dests_python_app/ten_packages/system/ten_runtime/lib",
        )
    else:
        # client depends on some libraries in the TEN app.
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "start_graph_and_set_dests_python_app/ten_packages/system/ten_runtime/lib",
        )

    my_env["LD_PRELOAD"] = ""

    client = subprocess.Popen(
        client_cmd, stdout=stdout, stderr=subprocess.STDOUT, env=my_env
    )

    client_rc = client.wait()
    if client_rc != 0:
        server.kill()

    sock.close()

    server_rc = server.wait()
    print("server: ", server_rc)
    print("client: ", client_rc)
    assert server_rc == 0
    assert client_rc == 0

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        fs_utils.remove_tree(app_root_path)
