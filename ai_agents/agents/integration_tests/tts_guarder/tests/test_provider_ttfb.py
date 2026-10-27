"""Second-stage live TTFB verification with mandatory independent hooks."""

import json
from pathlib import Path

import pytest

from .provider_ttfb_hooks import HOOKS, install_hook


@pytest.mark.api
def test_sequential_requests_ttfb_with_provider_hook(
    extension_name, config_dir, monkeypatch, record_property
):
    if extension_name not in HOOKS:
        pytest.skip("This case covers the thirteen second-stage adapters")
    from integration_tests.tts_http_timing.tests.boundary import SequentialTTFBTester

    config = json.loads(
        (Path(config_dir) / "property_basic_audio_setting1.json").read_text()
    )
    with install_hook(extension_name, config, monkeypatch) as reference:
        record_property("ttfb_provider_reference", "enabled")
        tester = SequentialTTFBTester(reference, reference_tolerance_ms=1)
        tester.set_test_mode_single(extension_name, json.dumps(config))
        error = tester.run()
        assert error is None, error.error_message() if error else ""
