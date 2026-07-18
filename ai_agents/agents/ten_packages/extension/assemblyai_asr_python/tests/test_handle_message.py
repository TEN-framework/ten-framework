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


class _RaisingWebSocket:
    """Async-iterable websocket whose iteration raises the given exception."""

    def __init__(self, exc: BaseException) -> None:
        self._exc = exc

    def __aiter__(self):
        return self

    async def __anext__(self):
        raise self._exc


def test_connection_closed_passes_string_args_to_on_error():
    """
    On a WebSocket close, on_error must receive string arguments. AssemblyAI v3
    reports errors via close codes (there is no in-band error message type), and
    on_error feeds ModuleError(message=str)/ModuleErrorVendorInfo(code=str), so
    passing the raw exception/int code would raise a Pydantic validation error.
    """
    from websockets.exceptions import ConnectionClosed
    from websockets.frames import Close

    callback = _RecordingCallback()
    recognition = _make_recognition(callback)
    # websockets >=14 requires Close frame objects (rcvd, sent), not a string;
    # str(ConnectionClosed) then contains the "4001" code the handler parses.
    recognition.websocket = _RaisingWebSocket(
        ConnectionClosed(Close(4001, "some reason"), None)
    )

    asyncio.run(recognition._message_handler())

    assert len(callback.errors) == 1
    error_msg, error_code = callback.errors[0]
    assert isinstance(error_msg, str)
    assert isinstance(error_code, str)
    assert error_code == "4001"
