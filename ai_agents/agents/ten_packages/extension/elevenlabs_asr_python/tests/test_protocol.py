import asyncio
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock

import pytest
from websockets.datastructures import Headers
from websockets.exceptions import InvalidStatus
from websockets.http11 import Response

from ten_ai_base.message import ModuleErrorCode
from ten_ai_base.timeline import AudioTimeline

from .. import recognition
from ..config import ElevenLabsASRConfig
from ..extension import ElevenLabsASRExtension
from ..recognition import ElevenLabsWSRecognition
from ..reconnect_manager import ReconnectManager


class FakeEnv:
    def __init__(self):
        self.events = []

    async def send_data(self, data):
        # Delivery must yield to expose cancellation during finalize_end.
        await asyncio.sleep(0)
        payload, error = data.get_property_to_json()
        assert error is None
        self.events.append((data.get_name(), json.loads(payload)))

    def log_debug(self, *args, **kwargs):
        del args, kwargs

    def log_info(self, *args, **kwargs):
        del args, kwargs

    def log_warn(self, *args, **kwargs):
        del args, kwargs

    def log_error(self, *args, **kwargs):
        del args, kwargs


def test_config_defaults_and_redaction():
    config = ElevenLabsASRConfig(
        params={
            "api_key": "secret",
            "token": "session-token",
            "ws_url": (
                "wss://api.elevenlabs.io/v1/speech-to-text/realtime"
                "?token=url-token"
            ),
        }
    )
    config.validate_config()

    assert config.params["model_id"] == "scribe_v2_realtime"
    assert config.params["ws_url"].startswith("wss://")
    assert config.params["include_timestamps"] is False
    assert config.params["commit_strategy"] == "vad"
    assert config.params["vad_silence_threshold_secs"] == 0.35
    assert "secret" not in config.to_json()
    assert "session-token" not in config.to_json()
    assert "url-token" not in config.to_json()


def test_config_rejects_legacy_model():
    config = ElevenLabsASRConfig(
        params={"api_key": "secret", "model_id": "scribe_v2"}
    )

    with pytest.raises(ValueError, match="scribe_v2_realtime"):
        config.validate_config()


def test_websocket_url_excludes_credentials_and_audio_rate():
    recognition = ElevenLabsWSRecognition(
        audio_timeline=AudioTimeline(),
        ten_env=FakeEnv(),
        params={
            "api_key": "secret",
            "model_id": "scribe_v2_realtime",
            "audio_format": "pcm_16000",
            "sample_rate": 16000,
            "keyterms": ["hello world"],
            "connect_timeout": 10,
            "finalize_timeout": 5,
        },
    )

    url = recognition._build_websocket_url()

    assert "secret" not in url
    assert "sample_rate" not in url
    assert "connect_timeout" not in url
    assert "finalize_timeout" not in url
    assert "keyterms=hello+world" in url


@pytest.mark.parametrize(
    "status_code, fatal",
    [(401, True), (403, True), (429, False), (500, False)],
)
def test_http_handshake_failure_classification(monkeypatch, status_code, fatal):
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(params={"api_key": "secret"})
    extension.config.validate_config()
    schedule_reconnect = Mock()
    extension._schedule_reconnect = schedule_reconnect
    connect = AsyncMock(
        side_effect=InvalidStatus(Response(status_code, "Rejected", Headers()))
    )
    monkeypatch.setattr(recognition.websockets, "connect", connect)

    async def run_test():
        await extension.start_connection()
        if fatal:
            # A later trigger_connect must respect the fatal latch.
            await extension.start_connection()

    asyncio.run(run_test())

    errors = [
        payload for name, payload in extension.ten_env.events if name == "error"
    ]
    assert len(errors) == 1
    expected_code = (
        ModuleErrorCode.FATAL_ERROR
        if fatal
        else ModuleErrorCode.NON_FATAL_ERROR
    )
    assert str(errors[0]["code"]) == expected_code.value
    assert errors[0]["vendor_info"]["code"] == str(status_code)
    assert extension._fatal_error is fatal
    connect.assert_awaited_once()
    if fatal:
        schedule_reconnect.assert_not_called()
    else:
        schedule_reconnect.assert_called_once_with()


def test_timestamp_event_does_not_duplicate_final_result():
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(
        params={
            "api_key": "secret",
            "model_id": "scribe_v2_realtime",
            "include_timestamps": True,
        }
    )
    extension.config.validate_config()
    extension.send_asr_result = AsyncMock()

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"},
            generation=0,
        )
        await extension.on_result(
            {
                "message_type": "committed_transcript_with_timestamps",
                "text": "hello",
                "language_code": "en",
                "words": [
                    {
                        "type": "word",
                        "text": "hello",
                        "start": 0,
                        "end": 0.5,
                    }
                ],
            },
            generation=0,
        )

    asyncio.run(run_test())

    extension.send_asr_result.assert_awaited_once()
    result = extension.send_asr_result.await_args.args[0]
    assert result.final is True
    assert result.language == "en-US"
    assert result.words[0].duration_ms == 500


@pytest.mark.parametrize("reconnect_offset", [0, 5000])
def test_word_timestamps_include_reconnect_offset(reconnect_offset):
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(
        params={"api_key": "secret", "include_timestamps": True}
    )
    extension.config.validate_config()
    extension.sent_user_audio_duration_ms_before_last_reset = reconnect_offset
    extension.audio_timeline.add_user_audio(1000)

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"}, 0
        )
        await extension.on_result(
            {
                "message_type": "committed_transcript_with_timestamps",
                "text": "hello",
                "words": [{"text": "hello", "start": 0.1, "end": 0.6}],
            },
            0,
        )

    asyncio.run(run_test())

    result = next(
        payload
        for name, payload in extension.ten_env.events
        if name == "asr_result"
    )
    assert result["start_ms"] == reconnect_offset + 100
    assert result["words"][0]["start_ms"] == result["start_ms"]
    assert result["duration_ms"] == 500
    assert result["words"][0]["duration_ms"] == 500


def test_finalize_completes_when_disconnected():
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(params={"api_key": "secret"})
    extension.config.validate_config()
    extension.send_asr_finalize_end = AsyncMock()

    asyncio.run(extension.finalize("session"))

    extension.send_asr_finalize_end.assert_awaited_once()


def test_multiple_commits_keep_their_timestamp_pairing():
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(
        params={"api_key": "secret", "include_timestamps": True}
    )
    extension.config.validate_config()
    extension.send_asr_result = AsyncMock()

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "first"},
            generation=0,
        )
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "second"},
            generation=0,
        )
        for text in ("first", "second"):
            await extension.on_result(
                {
                    "message_type": "committed_transcript_with_timestamps",
                    "text": text,
                    "language_code": "en",
                    "words": [],
                },
                generation=0,
            )

    asyncio.run(run_test())

    assert [
        call.args[0].text for call in extension.send_asr_result.await_args_list
    ] == ["first", "second"]


def make_timestamp_extension():
    extension = ElevenLabsASRExtension("elevenlabs")
    extension.ten_env = FakeEnv()
    extension.config = ElevenLabsASRConfig(
        params={"api_key": "secret", "include_timestamps": True}
    )
    extension.config.validate_config()
    extension.session_id = "session"
    extension.metadata = {"session_id": "session"}
    return extension


def test_delayed_timestamps_do_not_duplicate_final_result():
    extension = make_timestamp_extension()

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"}, 0
        )
        await extension._pending_timestamp_task
        await extension.on_result(
            {
                "message_type": "committed_transcript_with_timestamps",
                "text": "hello",
                "language_code": "en",
                "words": [],
            },
            0,
        )

    asyncio.run(run_test())

    results = [
        payload
        for name, payload in extension.ten_env.events
        if name == "asr_result"
    ]
    assert len(results) == 1
    assert results[0]["text"] == "hello"
    assert results[0]["final"] is True
    assert results[0]["metadata"]["session_id"] == "session"


def test_pending_timestamps_are_discarded_on_reconnect():
    extension = make_timestamp_extension()
    extension._connection_epoch = 1

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "old"}, 1
        )
        extension._connection_epoch = 2
        await extension.on_open(2, "new-session", 1)
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "new"}, 2
        )
        await extension._pending_timestamp_task

    asyncio.run(run_test())

    results = [
        payload
        for name, payload in extension.ten_env.events
        if name == "asr_result"
    ]
    assert [result["text"] for result in results] == ["new"]
    assert results[0]["metadata"]["asr_info"]["vendor_session_id"] == (
        "new-session"
    )


def test_late_timestamps_do_not_consume_next_identical_commit():
    extension = make_timestamp_extension()
    extension.audio_timeline.add_user_audio(1500)

    async def run_test():
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"}, 0
        )
        await extension._pending_timestamp_task
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"}, 0
        )
        for start in (0, 1):
            await extension.on_result(
                {
                    "message_type": "committed_transcript_with_timestamps",
                    "text": "hello",
                    "language_code": "en",
                    "words": [
                        {
                            "text": "hello",
                            "start": start,
                            "end": start + 0.5,
                        }
                    ],
                },
                0,
            )

    asyncio.run(run_test())

    results = [
        payload
        for name, payload in extension.ten_env.events
        if name == "asr_result"
    ]
    assert len(results) == 2
    assert results[0]["id"] != results[1]["id"]
    assert results[0]["words"] == []
    assert results[1]["words"][0]["start_ms"] == 1000
    assert results[1]["words"][0]["duration_ms"] == 500


def test_timestamp_fallback_delivers_finalize_end():
    extension = make_timestamp_extension()
    extension.recognition = SimpleNamespace(
        is_connected=lambda: True, send_final=AsyncMock(return_value=True)
    )
    extension.finalize_id = "finalize-1"
    extension._finalize_metadata = {"session_id": "finalize-session"}

    async def run_test():
        finalize_task = asyncio.create_task(extension.finalize("session"))
        await asyncio.sleep(0)
        await extension.on_result(
            {"message_type": "committed_transcript", "text": "hello"}, 0
        )
        await asyncio.wait_for(finalize_task, timeout=1)

    asyncio.run(run_test())

    events = [
        (name, payload)
        for name, payload in extension.ten_env.events
        if name in {"asr_result", "asr_finalize_end"}
    ]
    assert [name for name, _ in events] == ["asr_result", "asr_finalize_end"]
    assert events[1][1] == {
        "finalize_id": "finalize-1",
        "metadata": {"session_id": "finalize-session"},
    }


def test_reconnect_backoff_stays_capped_after_many_failures(monkeypatch):
    sleep = AsyncMock()
    monkeypatch.setattr(asyncio, "sleep", sleep)
    manager = ReconnectManager()
    connect = AsyncMock(side_effect=ConnectionError("unavailable"))

    async def run_test():
        for _ in range(1030):
            assert await manager.handle_reconnect(connect) is False

    asyncio.run(run_test())

    delays = [call.args[0] for call in sleep.await_args_list]
    assert delays[:4] == [0.5, 1.0, 2.0, 4.0]
    assert delays[4:] == [4.0] * 1026
    assert manager.attempts == 1030
    assert manager.can_retry()


def test_successful_handshake_resets_reconnect_backoff(monkeypatch):
    sleep = AsyncMock()
    monkeypatch.setattr(asyncio, "sleep", sleep)
    manager = ReconnectManager()

    async def run_test():
        for _ in range(5):
            await manager.handle_reconnect(AsyncMock())
        manager.mark_connection_successful()
        assert manager.attempts == 0
        await manager.handle_reconnect(AsyncMock())

    asyncio.run(run_test())

    assert sleep.await_args.args == (0.5,)
    assert manager.attempts == 1
