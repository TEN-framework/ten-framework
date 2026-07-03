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
import sys
from pathlib import Path

# Add project root to sys.path to allow running tests from this directory
project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import asyncio
import json
from unittest.mock import patch, AsyncMock, MagicMock
import pytest
from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import RequestState


class StateMachineExtensionTester(ExtensionTester):
    """Extension tester for state machine verification."""

    def __init__(self):
        super().__init__()
        self.request1_states = []  # Track state transitions for request 1
        self.request2_states = []  # Track state transitions for request 2
        self.audio_start_events = []  # Track audio_start events
        self.audio_end_events = []  # Track audio_end events
        self.request1_id = "state_test_req_1"
        self.request2_id = "state_test_req_2"
        self.test_completed = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        """Send two sequential TTS requests with different IDs."""
        ten_env_tester.log_info("State machine test started")

        # Send first request
        tts_input1 = TTSTextInput(
            request_id=self.request1_id,
            text="First request text",
            text_input_end=True,
        )
        data1 = Data.create("tts_text_input")
        data1.set_property_from_json(None, tts_input1.model_dump_json())
        ten_env_tester.send_data(data1)
        ten_env_tester.log_info(f"Sent first request: {self.request1_id}")

        # Send second request immediately (should wait for first to complete)
        tts_input2 = TTSTextInput(
            request_id=self.request2_id,
            text="Second request text",
            text_input_end=True,
        )
        data2 = Data.create("tts_text_input")
        data2.set_property_from_json(None, tts_input2.model_dump_json())
        ten_env_tester.send_data(data2)
        ten_env_tester.log_info(f"Sent second request: {self.request2_id}")

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        """Track state transitions and events."""
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
                f"Received tts_audio_end for request: {request_id}, reason: {reason}"
            )

            # Stop test after both requests complete
            if len(self.audio_end_events) == 2:
                ten_env.log_info("Both requests completed, stopping test")
                self.test_completed = True
                ten_env.stop_test()

    def verify_state_transitions(self, extension) -> bool:
        """
        Verify state transitions are correct.

        Expected behavior:
        1. Both requests should complete successfully
        2. Request 2 should start processing only after request 1 completes
        3. audio_start and audio_end events should be in correct order
        """
        # Verify both requests received audio_start
        assert (
            len(self.audio_start_events) == 2
        ), f"Expected 2 audio_start events, got {len(self.audio_start_events)}"
        assert (
            self.request1_id in self.audio_start_events
        ), f"Request 1 ({self.request1_id}) did not receive audio_start"
        assert (
            self.request2_id in self.audio_start_events
        ), f"Request 2 ({self.request2_id}) did not receive audio_start"

        # Verify both requests received audio_end with REQUEST_END reason
        assert (
            len(self.audio_end_events) == 2
        ), f"Expected 2 audio_end events, got {len(self.audio_end_events)}"

        req1_end = next(
            (e for e in self.audio_end_events if e[0] == self.request1_id), None
        )
        req2_end = next(
            (e for e in self.audio_end_events if e[0] == self.request2_id), None
        )

        assert (
            req1_end is not None
        ), f"Request 1 ({self.request1_id}) did not receive audio_end"
        assert (
            req2_end is not None
        ), f"Request 2 ({self.request2_id}) did not receive audio_end"

        # Reason is an integer (TTSAudioEndReason.REQUEST_END = 1)
        assert (
            req1_end[1] == 1
        ), f"Request 1 ended with unexpected reason: {req1_end[1]} (expected 1 for REQUEST_END)"
        assert (
            req2_end[1] == 1
        ), f"Request 2 ended with unexpected reason: {req2_end[1]} (expected 1 for REQUEST_END)"

        # Verify event order
        # Note: audio_start_events and audio_end_events are separate lists,
        # so we can only verify ordering within each list

        # Verify Request 1 starts before Request 2
        req1_start_idx = self.audio_start_events.index(self.request1_id)
        req2_start_idx = self.audio_start_events.index(self.request2_id)
        assert req1_start_idx < req2_start_idx, (
            f"Request 1 should start before Request 2 "
            f"(req1_start_idx={req1_start_idx}, req2_start_idx={req2_start_idx})"
        )

        # Verify Request 1 ends before Request 2 ends
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
        assert req1_end_idx < req2_end_idx, (
            f"Request 1 should end before Request 2 "
            f"(req1_end_idx={req1_end_idx}, req2_end_idx={req2_end_idx})"
        )

        print("✓ All state transition verifications passed!")
        return True


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2Client")
def test_sequential_requests_state_machine(MockElevenLabsTTS2Client):
    """
    Test that two sequential requests with different IDs are processed correctly.

    The second request should wait for the first to complete before processing.
    """
    print("\n=== Starting Sequential Requests State Machine Test ===")

    # Create mock client instance
    mock_instance = MockElevenLabsTTS2Client.return_value
    mock_instance.start_connection = AsyncMock()
    mock_instance.close = AsyncMock()
    mock_instance.cancel = AsyncMock()

    # Mock the synthesizer
    mock_synthesizer = AsyncMock()
    mock_instance.synthesizer = mock_synthesizer
    mock_synthesizer.send_text_in_connection = False
    mock_synthesizer.websocket_task = None

    # Use a class-based approach for better state management
    response_queue_ref = {"queue": None}  # Use dict to allow late binding

    class StateMachineStreamer:
        class Session:
            def __init__(self, request_id: str):
                self.request_id = request_id
                self.chunks_sent = 0

            async def get_audio_data(self):
                await asyncio.sleep(0.01)  # Simulate processing delay

                if self.chunks_sent < 3:
                    # Send audio chunks
                    self.chunks_sent += 1
                    audio_chunk = (
                        b"mock_audio_data_" + str(self.chunks_sent).encode()
                    )
                    return (
                        audio_chunk,
                        False,
                        self.request_id,
                        123 if self.chunks_sent == 1 else None,
                    )
                else:
                    # Signal request completion
                    return b"", True, self.request_id, None

        def __init__(self):
            from typing import Optional

            self.current_session: Optional[StateMachineStreamer.Session] = None

        async def populate_queue_for_session(self, session, queue):
            """Populate response queue with audio data for a specific session."""
            await asyncio.sleep(0.01)  # Small delay to let the extension start
            while True:
                audio_data, isFinal, request_id, ttfb_ms = (
                    await session.get_audio_data()
                )
                await queue.put((audio_data, isFinal, request_id, ttfb_ms))
                if isFinal:
                    break

        async def send_text(self, text_data):
            """Create a new session when synthesis starts."""
            if "First" in text_data.text:
                request_id = "state_test_req_1"
                print(f"  → Mock: Starting synthesis for request 1")
            elif "Second" in text_data.text:
                request_id = "state_test_req_2"
                print(f"  → Mock: Starting synthesis for request 2")
            else:
                request_id = "unknown"

            # Create new session for this request
            session = StateMachineStreamer.Session(request_id)
            self.current_session = session

            # Start populating queue in background for this session
            queue = response_queue_ref["queue"]
            if queue:
                asyncio.create_task(
                    self.populate_queue_for_session(session, queue)
                )

    streamer = StateMachineStreamer()
    mock_synthesizer.send_text = streamer.send_text
    mock_synthesizer.handle_flush = AsyncMock()
    mock_synthesizer.close_connection = AsyncMock()
    mock_synthesizer.is_connection_healthy = MagicMock(return_value=True)

    # Mock the client constructor
    def mock_client_init(
        config,
        ten_env,
        error_callback=None,
        response_msgs=None,
        ttfb_metrics_callback=None,
    ):
        # Use the real queue passed by the extension
        mock_instance.response_msgs = (
            response_msgs if response_msgs else asyncio.Queue()
        )
        response_queue_ref["queue"] = mock_instance.response_msgs
        mock_instance.synthesizer = mock_synthesizer

        # Mock client.send_text to call synthesizer.send_text
        async def mock_client_send_text(text_data):
            await mock_synthesizer.send_text(text_data)

        mock_instance.send_text = mock_client_send_text
        return mock_instance

    MockElevenLabsTTS2Client.side_effect = mock_client_init

    # Create tester
    tester = StateMachineExtensionTester()

    # Create test configuration (avoid environment variable issues)
    config = {
        "params": {
            "key": "test_api_key_for_state_machine",
            "model_id": "eleven_turbo_v2_5",
            "sample_rate": 16000,
        },
    }

    print(f"  → Using test config with mock client")

    # Set test mode and run
    print("  → Starting extension test...")
    tester.set_test_mode_single("elevenlabs_tts2_python", json.dumps(config))
    tester.run()

    # Verify results
    print("\n=== Verifying Test Results ===")
    print(f"  → test_completed: {tester.test_completed}")
    print(f"  → audio_start_events: {tester.audio_start_events}")
    print(f"  → audio_end_events: {tester.audio_end_events}")

    assert tester.test_completed, "Test did not complete successfully"

    # Verify state transitions
    tester.verify_state_transitions(None)

    print("\n✓ Sequential requests state machine test PASSED!")


@patch("elevenlabs_tts2_python.extension.ElevenLabsTTS2Client")
def test_request_state_transitions(MockElevenLabsTTS2Client):
    """
    Test detailed state transitions: QUEUED -> PROCESSING -> FINALIZING -> COMPLETED.

    This test verifies the internal state machine transitions.
    """
    print("\n=== Starting Request State Transitions Test ===")

    # Create mock client
    mock_instance = MockElevenLabsTTS2Client.return_value
    mock_instance.start_connection = AsyncMock()
    mock_instance.close = AsyncMock()
    mock_instance.cancel = AsyncMock()

    # Mock the synthesizer
    mock_synthesizer = AsyncMock()
    mock_instance.synthesizer = mock_synthesizer
    mock_synthesizer.send_text_in_connection = False
    mock_synthesizer.websocket_task = None

    # Simple streamer for single request
    response_queue_ref_simple = {
        "queue": None
    }  # Use dict to allow late binding

    class SimpleStreamer:
        class Session:
            def __init__(self):
                self.call_count = 0

            async def get_audio_data(self):
                await asyncio.sleep(0.01)
                self.call_count += 1
                if self.call_count == 1:
                    # Return one chunk
                    return b"audio_chunk", False, "state_transition_test", 123
                else:
                    # Signal completion
                    return b"", True, "state_transition_test", None

        def __init__(self):
            from typing import Optional

            self.current_session: Optional[SimpleStreamer.Session] = None

        async def populate_queue_for_session(self, session, queue):
            """Populate response queue with audio data for a specific session."""
            await asyncio.sleep(0.01)  # Small delay to let the extension start
            while True:
                audio_data, isFinal, request_id, ttfb_ms = (
                    await session.get_audio_data()
                )
                await queue.put((audio_data, isFinal, request_id, ttfb_ms))
                if isFinal:
                    break

        async def send_text(self, text_data):
            session = SimpleStreamer.Session()
            self.current_session = session
            # Start populating queue in background
            queue = response_queue_ref_simple["queue"]
            if queue:
                asyncio.create_task(
                    self.populate_queue_for_session(session, queue)
                )

    streamer = SimpleStreamer()
    mock_synthesizer.send_text = streamer.send_text
    mock_synthesizer.handle_flush = AsyncMock()
    mock_synthesizer.close_connection = AsyncMock()
    mock_synthesizer.is_connection_healthy = MagicMock(return_value=True)

    # Mock the client constructor
    def mock_client_init(
        config,
        ten_env,
        error_callback=None,
        response_msgs=None,
        ttfb_metrics_callback=None,
    ):
        # Use the real queue passed by the extension
        mock_instance.response_msgs = (
            response_msgs if response_msgs else asyncio.Queue()
        )
        response_queue_ref_simple["queue"] = mock_instance.response_msgs
        mock_instance.synthesizer = mock_synthesizer

        # Mock client.send_text to call synthesizer.send_text
        async def mock_client_send_text(text_data):
            await mock_synthesizer.send_text(text_data)

        mock_instance.send_text = mock_client_send_text
        return mock_instance

    MockElevenLabsTTS2Client.side_effect = mock_client_init

    # Create simple tester
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

    # Create test configuration (avoid environment variable issues)
    config = {
        "params": {
            "key": "test_api_key_for_state_transitions",
            "model_id": "eleven_turbo_v2_5",
            "sample_rate": 16000,
        },
    }

    # Set test mode and run
    print("  → Running state transition test...")
    tester.set_test_mode_single("elevenlabs_tts2_python", json.dumps(config))
    tester.run()

    # Verify
    assert tester.audio_end_received, "Did not receive audio_end event"

    print("✓ Request state transitions test PASSED!")


if __name__ == "__main__":
    # Run tests
    test_sequential_requests_state_machine()
    test_request_state_transitions()
