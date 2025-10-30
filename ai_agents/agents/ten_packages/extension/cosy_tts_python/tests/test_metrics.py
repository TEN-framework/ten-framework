#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from unittest.mock import patch, AsyncMock
import asyncio
import json

from ten_ai_base.struct import TTSTextInput
from ten_runtime import (
    Data,
    ExtensionTester,
    TenEnvTester,
)
from ..cosy_tts import (
    MESSAGE_TYPE_PCM,
    MESSAGE_TYPE_CMD_COMPLETE,
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


@patch("cosy_tts_python.extension.CosyTTSClient")
def test_ttfb_metric_is_sent(MockCosyTTSClient):
    """
    Tests that a TTFB (Time To First Byte) metric is correctly sent after
    receiving the first audio chunk from the TTS service.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    # --- Mock Configuration ---
    mock_instance = MockCosyTTSClient.return_value
    mock_instance.synthesize_audio = AsyncMock()

    # Create state to hold the queue and task
    stream_state = {"queue": None, "task": None}

    async def get_audio_data():
        """Simulate async streaming data from queue"""
        # Lazy initialization: create queue and start producer on first call
        if stream_state["queue"] is None:
            stream_state["queue"] = asyncio.Queue()

            async def simulate_audio_stream():
                """Simulate TTS service sending data asynchronously"""
                queue = stream_state["queue"]
                # Delay to simulate network latency and TTS processing time
                await asyncio.sleep(0.25)  # 250ms TTFB

                # Send first audio chunk
                await queue.put((False, MESSAGE_TYPE_PCM, b"\x11\x22\x33"))

                # Simulate more chunks arriving
                await asyncio.sleep(0.05)
                await queue.put((False, MESSAGE_TYPE_PCM, b"\x44\x55\x66"))

                await asyncio.sleep(0.05)
                await queue.put((False, MESSAGE_TYPE_PCM, b"\x77\x88\x99"))

                # Send completion signal
                await asyncio.sleep(0.05)
                await queue.put((True, MESSAGE_TYPE_CMD_COMPLETE, None))

            # Start producer task in background
            stream_state["task"] = asyncio.create_task(simulate_audio_stream())

        return await stream_state["queue"].get()

    mock_instance.get_audio_data.side_effect = get_audio_data

    # --- Test Setup ---
    # A minimal config is needed for the extension to initialize correctly.
    metrics_config = {
        "params": {
            "api_key": "a_valid_key",
        }
    }
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single("cosy_tts_python", json.dumps(metrics_config))

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    # --- Assertions ---
    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "TTFB metric was not received."

    # Check if the TTFB value is reasonable. It should be around 250ms with the delay
    # we introduced. Allow 50ms margin for timing variations and system scheduling.
    assert (
        tester.ttfb_value >= 200
    ), f"Expected TTFB to be >= 200ms, but got {tester.ttfb_value}ms."

    print(f"✅ TTFB metric test passed. Received TTFB: {tester.ttfb_value}ms.")
