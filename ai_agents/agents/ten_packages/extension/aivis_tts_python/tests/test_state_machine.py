#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Test TTS state machine behavior for sequential requests."""
import sys
from pathlib import Path

project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import asyncio
import json
from unittest.mock import patch, AsyncMock
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput, TTS2HttpResponseEventType


class StateMachineExtensionTester(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_events = []
        self.audio_end_events = []
        self.request1_id = "state_test_req_1"
        self.request2_id = "state_test_req_2"
        self.test_completed = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("State machine test started")

        tts_input1 = TTSTextInput(
            request_id=self.request1_id,
            text="First request text",
            text_input_end=True,
        )
        data1 = Data.create("tts_text_input")
        data1.set_property_from_json(None, tts_input1.model_dump_json())
        ten_env_tester.send_data(data1)

        tts_input2 = TTSTextInput(
            request_id=self.request2_id,
            text="Second request text",
            text_input_end=True,
        )
        data2 = Data.create("tts_text_input")
        data2.set_property_from_json(None, tts_input2.model_dump_json())
        ten_env_tester.send_data(data2)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        name = data.get_name()

        if name == "tts_audio_start":
            payload, _ = data.get_property_to_json(None)
            payload_dict = (
                json.loads(payload) if isinstance(payload, str) else payload
            )
            request_id = payload_dict.get("request_id", "")
            self.audio_start_events.append(request_id)

        elif name == "tts_audio_end":
            payload, _ = data.get_property_to_json(None)
            payload_dict = (
                json.loads(payload) if isinstance(payload, str) else payload
            )
            request_id = payload_dict.get("request_id", "")
            reason = payload_dict.get("reason", "")
            self.audio_end_events.append((request_id, reason))

            if len(self.audio_end_events) == 2:
                self.test_completed = True
                ten_env.stop_test()


@patch("aivis_tts_python.extension.AivisTTSClient")
def test_sequential_requests_state_machine(MockAivisTTSClient):
    """Test that two sequential requests are processed correctly."""
    print("\n=== Starting Sequential Requests State Machine Test ===")

    mock_instance = MockAivisTTSClient.return_value
    mock_instance.cancel = AsyncMock()
    mock_instance.clean = AsyncMock()

    request_order = []

    async def mock_get(text: str, request_id: str):
        if "First" in text:
            request_order.append("request_1")
        elif "Second" in text:
            request_order.append("request_2")

        for i in range(3):
            await asyncio.sleep(0.01)
            yield (
                b"mock_audio_data_" + str(i + 1).encode(),
                TTS2HttpResponseEventType.RESPONSE,
            )
        yield (None, TTS2HttpResponseEventType.END)

    mock_instance.get.side_effect = mock_get

    tester = StateMachineExtensionTester()

    config = {
        "params": {
            "api_key": "test_api_key_for_state_machine",
            "model_uuid": "a59cb814-0083-4369-8542-f51a29e72af7",
            "language": "ja",
            "output_sampling_rate": 16000,
            "output_audio_channels": "mono",
            "use_ssml": False,
            "leading_silence_seconds": 0.0,
            "trailing_silence_seconds": 0.1,
            "base_url": "https://api.aivis-project.com",
        },
    }

    tester.set_test_mode_single("aivis_tts_python", json.dumps(config))
    tester.run()

    assert tester.test_completed, "Test did not complete successfully"
    assert (
        len(tester.audio_start_events) == 2
    ), f"Expected 2 audio_start events, got {len(tester.audio_start_events)}"
    assert tester.request1_id in tester.audio_start_events
    assert tester.request2_id in tester.audio_start_events
    assert (
        len(tester.audio_end_events) == 2
    ), f"Expected 2 audio_end events, got {len(tester.audio_end_events)}"

    req1_end = next(
        (e for e in tester.audio_end_events if e[0] == tester.request1_id), None
    )
    req2_end = next(
        (e for e in tester.audio_end_events if e[0] == tester.request2_id), None
    )
    assert req1_end is not None
    assert req2_end is not None
    # Reason "REQUEST_END" is the string serialised by the base class when the
    # client yields RESPONSE chunks normally and then END. We don't want to
    # depend on the integer enum value here.
    assert req1_end[1] in (1, "REQUEST_END", "INTERRUPTED"), (
        f"Request 1 ended with unexpected reason: {req1_end[1]}"
    )
    assert req2_end[1] in (1, "REQUEST_END", "INTERRUPTED"), (
        f"Request 2 ended with unexpected reason: {req2_end[1]}"
    )

    req1_start_idx = tester.audio_start_events.index(tester.request1_id)
    req2_start_idx = tester.audio_start_events.index(tester.request2_id)
    assert req1_start_idx < req2_start_idx

    req1_end_idx = next(
        i
        for i, e in enumerate(tester.audio_end_events)
        if e[0] == tester.request1_id
    )
    req2_end_idx = next(
        i
        for i, e in enumerate(tester.audio_end_events)
        if e[0] == tester.request2_id
    )
    assert req1_end_idx < req2_end_idx

    assert request_order == ["request_1", "request_2"], (
        f"Expected request_1 before request_2, got {request_order}"
    )

    print("\n✓ Sequential requests state machine test PASSED!")
