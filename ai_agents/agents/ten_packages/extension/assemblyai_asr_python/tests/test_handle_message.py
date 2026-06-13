#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from typing import Optional, Dict, Any

from ten_packages.extension.assemblyai_asr_python.recognition import (
    AssemblyAIWSRecognition,
    AssemblyAIWSRecognitionCallback,
)


class _FakeTenEnv:
    """Minimal stand-in for AsyncTenEnv: logging is a no-op."""

    def log_debug(self, *args, **kwargs) -> None:
        pass

    def log_info(self, *args, **kwargs) -> None:
        pass

    def log_warn(self, *args, **kwargs) -> None:
        pass

    def log_error(self, *args, **kwargs) -> None:
        pass


class _RecordingCallback(AssemblyAIWSRecognitionCallback):
    """Records which callbacks fire so the routing can be asserted."""

    def __init__(self) -> None:
        self.errors: list = []
        self.events: list = []
        self.results: list = []

    async def on_error(
        self, error_msg: str, error_code: Optional[str] = None
    ) -> None:
        self.errors.append((error_msg, error_code))

    async def on_event(self, message_data: Dict[str, Any]) -> None:
        self.events.append(message_data)

    async def on_result(self, message_data: Dict[str, Any]) -> None:
        self.results.append(message_data)


def _make_recognition(callback: _RecordingCallback) -> AssemblyAIWSRecognition:
    return AssemblyAIWSRecognition(
        api_key="fake_key",
        ten_env=_FakeTenEnv(),
        callback=callback,
    )


def test_speech_started_is_not_treated_as_error():
    """
    A 'SpeechStarted' message is a normal informational event from the
    AssemblyAI v3 streaming API, not an error. It must not be routed to
    on_error (which expects a string and builds a ModuleError(message=str)),
    otherwise a Pydantic validation error is raised for the dict payload.
    """
    callback = _RecordingCallback()
    recognition = _make_recognition(callback)

    message = json.dumps(
        {
            "type": "SpeechStarted",
            "audio_start_ms": 1234,
            "confidence": 0.38146,
        }
    )

    asyncio.run(recognition._handle_message(message))

    assert callback.errors == [], (
        "SpeechStarted should not be routed to on_error, "
        f"got: {callback.errors}"
    )
    assert len(callback.events) == 1
    assert callback.events[0]["type"] == "SpeechStarted"


def test_unknown_message_is_routed_to_event_not_error():
    """Any unrecognized message type is informational, not an error."""
    callback = _RecordingCallback()
    recognition = _make_recognition(callback)

    message = json.dumps({"type": "SomeFutureEvent", "foo": "bar"})

    asyncio.run(recognition._handle_message(message))

    assert callback.errors == []
    assert len(callback.events) == 1
    assert callback.events[0]["type"] == "SomeFutureEvent"
