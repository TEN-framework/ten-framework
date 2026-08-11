#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import sys
from pathlib import Path

project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

from pathlib import Path
import json
from unittest.mock import AsyncMock, patch
import asyncio

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType


class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_value = -1
        self.audio_frame_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Metrics test started, sending TTS request.")

        tts_input = TTSTextInput(
            request_id="tts_request_for_metrics",
            text="hello, this is a metrics test.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        ten_env.log_info(f"on_data name: {name}")
        if name == "metrics":
            json_str, _ = data.get_property_to_json(None)
            ten_env.log_info(f"Received metrics: {json_str}")
            metrics_data = json.loads(json_str)

            nested_metrics = metrics_data.get("metrics", {})
            if "ttfb" in nested_metrics:
                self.ttfb_received = True
                self.ttfb_value = nested_metrics.get("ttfb", -1)
                ten_env.log_info(
                    f"Received TTFB metric with value: {self.ttfb_value}"
                )

        elif name == "tts_audio_end":
            self.audio_end_received = True
            if self.ttfb_received:
                ten_env.log_info("Received tts_audio_end, stopping test.")
                ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        if not self.audio_frame_received:
            self.audio_frame_received = True
            ten_env.log_info("First audio frame received.")


@patch("aivis_tts_python.extension.AivisTTSClient")
def test_ttfb_metric_is_sent(MockAivisTTSClient):
    """Tests that a TTFB metric is correctly sent after the first audio chunk."""
    print("Starting test_ttfb_metric_is_sent with mock...")

    mock_instance = MockAivisTTSClient.return_value
    mock_instance.clean = AsyncMock()

    async def mock_get_audio_with_delay(
        text: str, request_id: str | None = None
    ):
        await asyncio.sleep(0.2)
        yield (b"\x11\x22\x33", TTS2HttpResponseEventType.RESPONSE)
        yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get_audio_with_delay

    metrics_config = {
        "params": {
            "api_key": "test_api_key",
            "model_uuid": "a59cb814-0083-4369-8542-f51a29e72af7",
        }
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single("aivis_tts_python", json.dumps(metrics_config))

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."

    assert (
        tester.ttfb_value >= 200
    ), f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")
