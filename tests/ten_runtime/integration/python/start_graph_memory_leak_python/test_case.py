"""
Test start_graph_memory_leak_python.

This test verifies that repeatedly starting and stopping subgraphs does not
cause continuous memory growth (memory leak). It runs CYCLE_COUNT iterations
of start_graph/stop_graph and checks whether the process RSS stays within an
acceptable bound.
"""

import subprocess
import os
import sys
from sys import stdout
from .utils import msgpack, build_config, build_pkg, fs_utils


def test_start_graph_memory_leak_python():
    """Test that repeated start/stop of subgraphs does not leak memory."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    my_env = os.environ.copy()

    my_env["PYTHONMALLOC"] = "malloc"
    my_env["PYTHONDEVMODE"] = "1"

    app_dir_name = "start_graph_memory_leak_python_app"
    app_root_path = os.path.join(base_path, app_dir_name)
    app_language = "python"

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

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

    print("Bootstrapping Python dependencies...")
    rc = build_pkg.bootstrap_python_dependencies(
        app_root_path, my_env, log_level=1
    )
    if rc != 0:
        assert False, "Failed to bootstrap Python dependencies."

    if sys.platform == "linux":
        if build_config_args.enable_sanitizer:
            libasan_path = os.path.join(
                base_path,
                (
                    "start_graph_memory_leak_python_app/ten_packages/system/"
                    "ten_runtime/lib/libasan.so"
                ),
            )

            if os.path.exists(libasan_path):
                print("Using AddressSanitizer library.")
                my_env["LD_PRELOAD"] = libasan_path

    main_py_path = os.path.join(app_root_path, "main.py")
    if not os.path.isfile(main_py_path):
        print(f"main.py not found at '{main_py_path}'.")
        assert False

    if sys.platform == "linux" and build_config_args.enable_sanitizer:
        print("Starting server with python from venv (bypassing uv run)...")
        venv_path = os.path.join(app_root_path, ".venv")
        python_exe = os.path.join(venv_path, "bin", "python")
        server_cmd = [python_exe, "main.py"]
        my_env["VIRTUAL_ENV"] = venv_path
    else:
        print("Starting server with uv run main.py...")
        server_cmd = ["uv", "run", "main.py"]

    if sys.platform == "win32":
        client_cmd = os.path.join(
            base_path, "start_graph_memory_leak_python_app_client.exe"
        )
    else:
        client_cmd = os.path.join(
            base_path, "start_graph_memory_leak_python_app_client"
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
            "The start_graph_memory_leak_python app did not start within 30 seconds."
        )

        server.kill()
        exit_code = server.wait()
        print(
            "The exit code of start_graph_memory_leak_python server: ",
            exit_code,
        )

        assert exit_code == 0
        assert False

    if sys.platform == "win32":
        my_env["PATH"] = (
            os.path.join(
                base_path,
                "start_graph_memory_leak_python_app/ten_packages/system/ten_runtime/lib",
            )
            + ";"
            + my_env["PATH"]
        )
    elif sys.platform == "darwin":
        my_env["DYLD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "start_graph_memory_leak_python_app/ten_packages/system/ten_runtime/lib",
        )
    else:
        my_env["LD_LIBRARY_PATH"] = os.path.join(
            base_path,
            "start_graph_memory_leak_python_app/ten_packages/system/ten_runtime/lib",
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
    print("server exit code: ", server_rc)
    print("client exit code: ", client_rc)
    assert server_rc == 0
    assert client_rc == 0

    if build_config_args.ten_enable_tests_cleanup is True:
        fs_utils.remove_tree(app_root_path)
