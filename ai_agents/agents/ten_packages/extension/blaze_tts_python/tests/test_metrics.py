#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch

from blaze_tts_python.tests.mock import (
    BASIC_CONFIG,
    RecordingTester,
    install_mock_client,
)


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_ttfb_metric_is_sent(MockBlazeTTSClient):
    install_mock_client(MockBlazeTTSClient, delay=0.2)

    tester = RecordingTester([("req-metrics", "hello", True)])
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    ttfb_values = [
        payload["metrics"]["ttfb"]
        for payload in tester.payloads("metrics")
        if "ttfb" in payload.get("metrics", {})
    ]
    assert len(ttfb_values) == 1
    # The mock waits 200 ms before the first chunk.
    assert ttfb_values[0] >= 150
