#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
from .utils import cmd_exec


def test_tman_tracing_flame_mode():
    """Test that tman --tracing flame install generates Flamegraph SVG file."""
    base_path = os.path.dirname(os.path.abspath(__file__))
    root_dir = os.path.join(base_path, "../../../../")

    if sys.platform == "win32":
        os.environ["PATH"] = (
            os.path.join(root_dir, "ten_manager/lib") + ";" + os.getenv("PATH")
        )
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman.exe")
    else:
        tman_bin = os.path.join(root_dir, "ten_manager/bin/tman")

    app_dir = os.path.join(base_path, "test_app")
    tracing_dir = os.path.join(app_dir, "tman_tracing")

    config_file = os.path.join(
        root_dir,
        "tests/local_registry/config.json",
    )

    # Execute tman install with --tracing flame
    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--tracing",
            "flame",
            "--yes",
            "install",
        ],
        cwd=app_dir,
    )
    if returncode != 0:
        print(output_text)
        assert False, "tman install with --tracing flame failed"

    # Check that tman_tracing directory exists
    assert os.path.exists(tracing_dir), "tman_tracing directory was not created"

    # Check that flamegraph SVG file exists
    flame_files = [f for f in os.listdir(tracing_dir) if f.startswith("flamegraph_") and f.endswith(".svg")]
    assert len(flame_files) > 0, "Flamegraph SVG file was not generated"

    # Validate SVG format (basic check)
    flame_file_path = os.path.join(tracing_dir, flame_files[0])
    with open(flame_file_path, 'r') as f:
        svg_content = f.read()
        assert svg_content.startswith("<?xml") or svg_content.startswith("<svg"), \
            "Flamegraph file does not appear to be valid SVG"
        assert "</svg>" in svg_content, "Flamegraph SVG is incomplete"
        print(f"✓ Flamegraph file validated: {flame_files[0]}")

    # Verify that chrome tracing files are NOT created in flame-only mode
    trace_files = [f for f in os.listdir(tracing_dir) if f.startswith("trace_") and f.endswith(".json")]
    assert len(trace_files) == 0, "Chrome tracing files should not be created in flame-only mode"

    print("✅ Flamegraph mode test passed")


if __name__ == "__main__":
    test_tman_tracing_flame_mode()
