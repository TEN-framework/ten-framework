#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import os
import sys
import json
from .utils import cmd_exec


def test_tman_tracing_all_mode():
    """Test that tman --tracing all install generates both Chrome tracing and Flamegraph files."""
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

    # Execute tman install with --tracing all
    returncode, output_text = cmd_exec.run_cmd_realtime(
        [
            tman_bin,
            f"--config-file={config_file}",
            "--tracing",
            "all",
            "--yes",
            "install",
        ],
        cwd=app_dir,
    )
    if returncode != 0:
        print(output_text)
        assert False, "tman install with --tracing all failed"

    # Check that tman_tracing directory exists
    assert os.path.exists(tracing_dir), "tman_tracing directory was not created"

    # Check that Chrome tracing JSON file exists
    trace_files = [f for f in os.listdir(tracing_dir) if f.startswith("trace_") and f.endswith(".json")]
    assert len(trace_files) > 0, "Chrome tracing JSON file was not generated"

    # Validate Chrome tracing JSON format
    trace_file_path = os.path.join(tracing_dir, trace_files[0])
    with open(trace_file_path, 'r') as f:
        try:
            trace_data = json.load(f)
            assert isinstance(trace_data, (list, dict)), "Invalid Chrome tracing JSON format"
            if isinstance(trace_data, dict):
                assert "traceEvents" in trace_data or "displayTimeUnit" in trace_data, \
                    "Chrome tracing JSON missing expected fields"
            print(f"✓ Chrome tracing file validated: {trace_files[0]}")
        except json.JSONDecodeError as e:
            assert False, f"Chrome tracing JSON file is not valid JSON: {e}"

    # Check that flamegraph SVG file exists
    flame_files = [f for f in os.listdir(tracing_dir) if f.startswith("flamegraph_") and f.endswith(".svg")]
    assert len(flame_files) > 0, "Flamegraph SVG file was not generated"

    # Validate flamegraph SVG format
    flame_file_path = os.path.join(tracing_dir, flame_files[0])
    with open(flame_file_path, 'r') as f:
        svg_content = f.read()
        assert svg_content.startswith("<?xml") or svg_content.startswith("<svg"), \
            "Flamegraph file does not appear to be valid SVG"
        assert "</svg>" in svg_content, "Flamegraph SVG is incomplete"
        print(f"✓ Flamegraph file validated: {flame_files[0]}")

    # Check that both files have matching timestamps (optional but good practice)
    trace_timestamp = trace_files[0].replace("trace_", "").replace(".json", "")
    flame_timestamp = flame_files[0].replace("flamegraph_", "").replace(".svg", "")
    assert trace_timestamp == flame_timestamp, \
        "Chrome tracing and flamegraph files should have matching timestamps"

    print("✅ All mode test passed - both Chrome tracing and Flamegraph files generated successfully")


if __name__ == "__main__":
    test_tman_tracing_all_mode()
