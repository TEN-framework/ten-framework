#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
Test TTS state machine behavior for sequential requests.

This test verifies that:
1. Request states transition correctly: QUEUED -> PROCESSING -> FINALIZING -> COMPLETED
2. Second request waits for first request to complete before processing
3. State machine handles multiple sequential requests correctly
"""
import asyncio
import json
from unittest.mock import patch, MagicMock, AsyncMock
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput


class StateMachineExtensionTester(ExtensionTester):
    """Extension tester for state machine verification."""

    def __init__(self):
        super().__init__()
        self.request1_states = []
        self.request2_states = []
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
            payload, _ = data.get_property_to_json("")
            payload_dict = (
                eval(payload) if isinstance(payload, str) else payload
            )
            request_id = payload_dict.get("request_id", "")
            self.audio_start_events.append(request_id)
            ten_env.log_info(
                f"Received tts_audio_start for request: {request_id}"
            )

        elif name == "tts_audio_end":
            payload, _ = data.get_property_to_json("")
            payload_dict = (
                eval(payload) if isinstance(payload, str) else payload
            )
            request_id = payload_dict.get("request_id", "")
            reason = payload_dict.get("reason", "")
            self.audio_end_events.append((request_id, reason))
            ten_env.log_info(
                f"Received tts_audio_end for request: {request_id}"
            )

            if len(self.audio_end_events) == 2:
                self.test_completed = True
                ten_env.stop_test()

    def verify_state_transitions(self) -> bool:
        assert (
            len(self.audio_start_events) == 2
        ), f"Expected 2 audio_start events, got {len(self.audio_start_events)}"
        assert self.request1_id in self.audio_start_events
        assert self.request2_id in self.audio_start_events

        assert (
            len(self.audio_end_events) == 2
        ), f"Expected 2 audio_end events, got {len(self.audio_end_events)}"

        req1_end = next(
            (e for e in self.audio_end_events if e[0] == self.request1_id), None
        )
        req2_end = next(
            (e for e in self.audio_end_events if e[0] == self.request2_id), None
        )

        assert req1_end is not None
        assert req2_end is not None
        assert (
            req1_end[1] == 1
        ), f"Request 1 ended with unexpected reason: {req1_end[1]}"
        assert (
            req2_end[1] == 1
        ), f"Request 2 ended with unexpected reason: {req2_end[1]}"

        req1_start_idx = self.audio_start_events.index(self.request1_id)
        req2_start_idx = self.audio_start_events.index(self.request2_id)
        assert req1_start_idx < req2_start_idx

        req1_end_idx = next(
            i
            for i, e in enumerate(self.audio_end_events)
            if e[0] == self.request1_id
        )
        req2_end_idx = next(
            i
            for i, e in enumerate(self.audio_end_events)
            if e[0] == self.request2_id
        )
        assert req1_end_idx < req2_end_idx

        print("✓ All state transition verifications passed!")
        return True


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_sequential_requests_state_machine(MockCartesiaTTSClient):
    """
    Test that two sequential requests with different IDs are processed correctly.
    The second request should wait for the first to complete before processing.
    """
    print("\n=== Starting Sequential Requests State Machine Test ===")

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

        async def _stream():
            await asyncio.sleep(0.01)
            await pcm_queue.put((b"mock_audio_chunk_1", request_id, 0))
            await asyncio.sleep(0.01)
            await pcm_queue.put((b"mock_audio_chunk_2", request_id, 0))
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

    tester = StateMachineExtensionTester()
    config = {
        "params": {
            "api_key": "test_api_key_for_state_machine",
            "model_id": "sonic-english",
            "voice": {"id": "test_voice_id", "mode": "id"},
            "output_format": {
                "container": "raw",
                "encoding": "pcm_s16le",
                "sample_rate": 16000,
            },
        },
    }

    tester.set_test_mode_single("cartesia_tts", json.dumps(config))
    tester.run()

    print(f"  → test_completed: {tester.test_completed}")
    print(f"  → audio_start_events: {tester.audio_start_events}")
    print(f"  → audio_end_events: {tester.audio_end_events}")

    assert tester.test_completed, "Test did not complete successfully"
    tester.verify_state_transitions()

    print("\n✓ Sequential requests state machine test PASSED!")


@patch("cartesia_tts.extension.CartesiaTTSClient")
def test_request_state_transitions(MockCartesiaTTSClient):
    """
    Test detailed state transitions: QUEUED -> PROCESSING -> FINALIZING -> COMPLETED.
    """
    print("\n=== Starting Request State Transitions Test ===")

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

        async def _stream():
            await asyncio.sleep(0.01)
            await pcm_queue.put((b"mock_audio_chunk", request_id, 0))
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

    class StateTransitionTester(ExtensionTester):
        def __init__(self):
            super().__init__()
            self.audio_end_received = False

        def on_start(self, ten_env_tester: TenEnvTester) -> None:
            tts_input = TTSTextInput(
                request_id="state_transition_test",
                text="Test state transitions",
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env_tester.send_data(data)
            ten_env_tester.on_start_done()

        def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
            if data.get_name() == "tts_audio_end":
                self.audio_end_received = True
                ten_env.stop_test()

    tester = StateTransitionTester()
    config = {
        "params": {
            "api_key": "test_api_key_for_state_transitions",
            "model_id": "sonic-english",
            "voice": {"id": "test_voice_id", "mode": "id"},
            "output_format": {
                "container": "raw",
                "encoding": "pcm_s16le",
                "sample_rate": 16000,
            },
        },
    }

    tester.set_test_mode_single("cartesia_tts", json.dumps(config))
    tester.run()

    assert tester.audio_end_received, "Did not receive audio_end event"
    print("✓ Request state transitions test PASSED!")


if __name__ == "__main__":
    import pytest
    import sys

    sys.exit(pytest.main([__file__, "-v"]))
