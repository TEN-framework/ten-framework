"""
Test standalone_vosk_asr_cpp.
"""

import subprocess
import os
from sys import stdout
import urllib.request
import zipfile
import io
from .utils import build_config, fs_utils


def test_standalone_vosk_asr_cpp():
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../../")

    extension_root_path = os.path.join(base_path, "vosk_asr_cpp")
    fs_utils.remove_tree(extension_root_path)

    my_env = os.environ.copy()

    # Step 1:
    #
    # Create vosk_asr_cpp package directly.
    tman_fetch_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "fetch",
        "extension",
        "vosk_asr_cpp",
    ]

    tman_fetch_process = subprocess.Popen(
        tman_fetch_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=base_path,
    )
    tman_fetch_process.wait()
    return_code = tman_fetch_process.returncode
    if return_code != 0:
        assert False, "Failed to fetch package."

    # Step 2:
    #
    # Install all the dependencies of the vosk_asr_cpp package.
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
    # Download and extract the vosk SDK.
    print("Downloading and extracting the vosk SDK.")

    zip_url = (
        "https://github.com/alphacep/vosk-api/releases/"
        "download/v0.3.45/vosk-linux-x86_64-0.3.45.zip"
    )
    try:
        response = urllib.request.urlopen(zip_url)
        zip_data = response.read()
    except Exception as e:
        assert False, f"Failed to download zip file: {e}"

    try:
        with zipfile.ZipFile(io.BytesIO(zip_data)) as zip_ref:
            include_dir = os.path.join(extension_root_path, "include")
            os.makedirs(include_dir, exist_ok=True)

            lib_private_dir = os.path.join(extension_root_path, "lib_private")
            os.makedirs(lib_private_dir, exist_ok=True)

            for zip_info in zip_ref.infolist():
                filename = os.path.basename(zip_info.filename)
                if filename == "vosk_api.h":
                    # Extract `vosk_api.h` directly into the `include/`
                    # directory without preserving the intermediate directory
                    # structure.
                    target_path = os.path.join(include_dir, "vosk_api.h")
                    with zip_ref.open(zip_info) as source, open(
                        target_path, "wb"
                    ) as target:
                        target.write(source.read())
                elif filename == "libvosk.so":
                    # Extract `libvosk.so` directly into the `lib_private/`
                    # directory without preserving the intermediate directory
                    # structure.
                    target_path = os.path.join(lib_private_dir, "libvosk.so")
                    with zip_ref.open(zip_info) as source, open(
                        target_path, "wb"
                    ) as target:
                        target.write(source.read())
    except Exception as e:
        assert False, f"Failed to extract zip file: {e}"

    # Step 4:
    #
    # Download and extract the vosk model.
    print("Downloading and extracting the vosk model.")

    model_zip_url = (
        "https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
    )
    try:
        response = urllib.request.urlopen(model_zip_url)
        model_zip_data = response.read()
    except Exception as e:
        assert False, f"Failed to download model zip file: {e}"

    try:
        with zipfile.ZipFile(io.BytesIO(model_zip_data)) as zip_ref:
            models_dir = os.path.join(extension_root_path, "models")
            os.makedirs(models_dir, exist_ok=True)
            zip_ref.extractall(models_dir)
    except Exception as e:
        assert False, f"Failed to extract model zip file: {e}"

    # Step 5:
    #
    # Build the extension.
    print("Building the extension.")

    tman_run_build_cmd = [
        os.path.join(root_dir, "ten_manager/bin/tman"),
        "--config-file",
        os.path.join(root_dir, "tests/local_registry/config.json"),
        "--yes",
        "run",
        "build",
    ]

    tman_run_build_process = subprocess.Popen(
        tman_run_build_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    tman_run_build_process.wait()
    return_code = tman_run_build_process.returncode
    if return_code != 0:
        assert False, "Failed to build package."

    build_config_args = build_config.parse_build_config(
        os.path.join(root_dir, "tgn_args.txt"),
    )

    # Step 6:
    #
    # Standalone testing the extension.
    print("Testing the extension.")

    standalone_test_cmd = [
        os.path.join(extension_root_path, "bin/vosk_asr_cpp_test"),
    ]

    standalone_test_process = subprocess.Popen(
        standalone_test_cmd,
        stdout=stdout,
        stderr=subprocess.STDOUT,
        env=my_env,
        cwd=extension_root_path,
    )
    standalone_test_process.wait()
    return_code = standalone_test_process.returncode
    if return_code != 0:
        assert False, "Failed to test package."

    if build_config_args.ten_enable_tests_cleanup is True:
        # Testing complete. If builds are only created during the testing phase,
        # we can clear the build results to save disk space.
        fs_utils.remove_tree(extension_root_path)
