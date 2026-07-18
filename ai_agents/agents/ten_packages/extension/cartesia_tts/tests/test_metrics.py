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
from pathlib import Path
import json
from unittest.mock import patch, AsyncMock, MagicMock
import asyncio

from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


# ================ test metrics ================
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


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_ttfb_metric_is_sent(MockCartesiaTTSClient):
    """
    Tests that a TTFB metric is correctly sent after receiving the first
    audio chunk from the TTS service.

    In the full-duplex architecture, TTFB is calculated inside the client's
    _receive_loop and reported via ttfb_metrics_callback. The extension then
    calls send_tts_ttfb_metrics. We mock the client to put audio into
    pcm_queue and verify the metrics event is emitted.
    """
    print("Starting test_ttfb_metric_is_sent with mock...")

    mock_instance = MagicMock()
    pcm_queue = asyncio.Queue()
    words_queue = asyncio.Queue()

    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()
    mock_instance.cancel = AsyncMock()
    mock_instance.set_current_request_id = AsyncMock()
    mock_instance.send_audio_end_signal = AsyncMock()

    async def mock_tts(t):
        request_id = t.request_id

        # Retrieve ttfb_metrics_callback passed to CartesiaTTSClient constructor
        ttfb_cb = MockCartesiaTTSClient.call_args.kwargs.get(
            "ttfb_metrics_callback"
        )

        async def _stream():
            await asyncio.sleep(0.01)
            # Simulate TTFB callback as real client does in _receive_loop
            if ttfb_cb:
                await ttfb_cb(request_id, 42)
            await pcm_queue.put((b"\x11\x22\x33", request_id, 0))
            await asyncio.sleep(0.01)
            await pcm_queue.put((None, request_id, 0))

        asyncio.create_task(_stream())

    mock_instance.text_to_speech = AsyncMock(side_effect=mock_tts)

    async def mock_get_audio():
        return await pcm_queue.get()

    mock_instance.get_audio = AsyncMock(side_effect=mock_get_audio)

    async def mock_get_words():
        return await words_queue.get()

    mock_instance.get_words = AsyncMock(side_effect=mock_get_words)

    MockCartesiaTTSClient.return_value = mock_instance

    metrics_config = {"params": {"api_key": "test_api_key"}}
    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single("cartesia_tts", json.dumps(metrics_config))

    print("Running TTFB metrics test...")
    tester.run()
    print("TTFB metrics test completed.")

    assert tester.audio_frame_received, "Did not receive any audio frame."
    assert tester.audio_end_received, "Did not receive the tts_audio_end event."
    assert tester.ttfb_received, "Did not receive TTFB metric."
    assert (
        tester.ttfb_value == 42
    ), f"Expected TTFB value 42, got {tester.ttfb_value}"

    print("✅ TTFB metric test passed.")
