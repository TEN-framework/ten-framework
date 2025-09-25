import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
# The project root is 6 levels up from the parent directory of this file.
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
from typing import Any
from unittest.mock import patch, AsyncMock
import asyncio

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from stepfun_tts_python.stepfun_tts import (
    EVENT_TTSTaskFinished,
    EVENT_TTSResponse,
)


# ================ test metrics ================
class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_value = -1
        self.audio_frame_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Called when test starts, sends a TTS request."""
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

            # According to the new structure, 'ttfb' is nested inside a 'metrics' object.
            nested_metrics = metrics_data.get("metrics", {})
            if "ttfb" in nested_metrics:
                self.ttfb_received = True
                self.ttfb_value = nested_metrics.get("ttfb", -1)
                ten_env.log_info(
                    f"Received TTFB metric with value: {self.ttfb_value}"
                )

        elif name == "tts_audio_end":
            self.audio_end_received = True
            # Stop the test only after both TTFB and audio end are received
            if self.ttfb_received:
                ten_env.log_info("Received tts_audio_end, stopping test.")
                ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        """Receives audio frames and confirms the stream is working."""
        if not self.audio_frame_received:
            self.audio_frame_received = True
            ten_env.log_info("First audio frame received.")


@patch("stepfun_tts_python.extension.StepFunTTSWebsocket")
def test_ttfb_metric_is_sent(MockStepFunTTSWebsocket):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockStepFunTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()

    # Store the callback that will be set by the extension
    metrics_audio_callback = None

    def capture_metrics_callback(*args, **kwargs):
        # Capture the on_audio_data callback from the keyword arguments
        nonlocal metrics_audio_callback
        if "on_audio_data" in kwargs:
            metrics_audio_callback = kwargs["on_audio_data"]
        return mock_instance

    MockStepFunTTSWebsocket.side_effect = capture_metrics_callback

    # Mock the callback-based audio handling for StepFun TTS with delay
    async def mock_get_audio_with_delay(tts_input):
        # Simulate network latency or processing time before the first byte
        await asyncio.sleep(0.2)

        if metrics_audio_callback:
            # Simulate sentence start
            await metrics_audio_callback(b"", 350, 0)  # EVENT_TTSSentenceStart

            # Simulate first audio chunk (this should trigger TTFB metric)
            await metrics_audio_callback(b"\x11\x22\x33", EVENT_TTSResponse, 0)

            # Simulate the end of the stream
            await metrics_audio_callback(b"", EVENT_TTSTaskFinished, 0)

    mock_instance.get = AsyncMock(side_effect=mock_get_audio_with_delay)

    # --- Test Setup ---
    # A minimal config is needed for the extension to initialize correctly.
    metrics_config = {
        "params": {
            "api_key": "a_valid_key",
            "model": "step-tts-mini",
            "voice_id": "cixingnansheng",
            "sample_rate": 24000,
        },
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "stepfun_tts_python", json.dumps(metrics_config)
    )

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    # --- Assertions ---
    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."

    # Check if the TTFB value is reasonable. It should be more than
    # the 0.2s delay we introduced. We check for >= 200ms.
    assert (
        tester.ttfb_value >= 200
    ), f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")
