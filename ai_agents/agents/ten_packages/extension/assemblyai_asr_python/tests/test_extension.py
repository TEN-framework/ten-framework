#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Behavioural tests for AssemblyAIASRExtension without a live connection.

The extension is driven through its vendor callbacks and ``on_data`` with a
recording TenEnv and a fake recognition client, so the TEN app runtime is not
required.
"""

import asyncio
import json
from typing import Any, Dict, List, Optional
from unittest.mock import AsyncMock, patch

import pytest
from ten_ai_base.message import ModuleConnectionStatus
from ten_runtime import Data

from .. import extension as extension_module
from ..config import AssemblyAIASRConfig
from ..extension import AssemblyAIASRExtension
from ..recognition import AssemblyAIConnectionError
from ..reconnect_manager import ReconnectManager

FATAL = -1000
NON_FATAL = 1000


class _FakeTenEnv:
    """Records everything the extension emits."""

    def __init__(self) -> None:
        self.sent: List[Data] = []
        self.logs: List[str] = []

    def _log(self, msg: str, **_: Any) -> None:
        self.logs.append(msg)

    log_debug = log_info = log_warn = log_error = _log

    async def send_data(self, data: Data) -> None:
        self.sent.append(data)

    def payloads(self, name: str) -> List[Dict[str, Any]]:
        out = []
        for data in self.sent:
            if data.get_name() == name:
                raw, _ = data.get_property_to_json()
                out.append(json.loads(raw))
        return out


class _FakeRecognition:
    def __init__(self, connected: bool = True) -> None:
        self.connected = connected
        self.updates: List[Dict[str, Any]] = []
        self.force_endpoints = 0
        self.closed = False
        self.session_id = "sess-1"
        self.started = 0

    async def start(self) -> bool:
        self.started += 1
        return True

    def is_connected(self) -> bool:
        return self.connected

    async def send_update_configuration(self, update: Dict[str, Any]) -> bool:
        self.updates.append(update)
        return True

    async def force_endpoint(self) -> bool:
        self.force_endpoints += 1
        return True

    async def close(self) -> None:
        self.closed = True
        self.connected = False


def _make_extension(
    connected: bool = True, mock_reconnect: bool = True, **params: Any
) -> tuple[AssemblyAIASRExtension, _FakeTenEnv, _FakeRecognition]:
    ext = AssemblyAIASRExtension("test")
    env = _FakeTenEnv()
    ext.ten_env = env  # type: ignore[assignment]
    config = AssemblyAIASRConfig.model_validate(
        {"params": {"api_key": "k", **params}}
    )
    config.update(config.params)
    ext.config = config
    ext.reconnect_manager = ReconnectManager(logger=env)
    recognition = _FakeRecognition(connected=connected)
    ext.recognition = recognition  # type: ignore[assignment]
    if mock_reconnect:
        ext._handle_reconnect = AsyncMock()  # type: ignore[method-assign]
    return ext, env, recognition


def _tts_text_input(
    request_id: str, text: str, end: bool, session_id: str = "123"
) -> Data:
    data = Data.create("tts_text_input")
    data.set_property_from_json(
        None,
        json.dumps(
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": end,
                "metadata": {"session_id": session_id},
            }
        ),
    )
    return data


def _turn(**overrides: Any) -> Dict[str, Any]:
    turn = {
        "type": "Turn",
        "turn_order": 3,
        "turn_is_formatted": True,
        "end_of_turn": True,
        "transcript": "Hello world.",
        "end_of_turn_confidence": 0.93,
        "words": [
            {
                "text": "Hello",
                "start": 1200,
                "end": 1500,
                "confidence": 0.99,
                "word_is_final": True,
            },
            {
                "text": "world.",
                "start": 1600,
                "end": 1900,
                "confidence": 0.98,
                "word_is_final": True,
            },
        ],
    }
    turn.update(overrides)
    return turn


# --------------------------------------------------------------------------
# Vendor metadata / features
# --------------------------------------------------------------------------


def test_vendor_metadata_reports_key_url_model_and_mode():
    ext, _, _ = _make_extension(mode="min_latency")

    assert ext.vendor_metadata() == {
        "key": "k",
        "url": "wss://streaming.assemblyai.com/v3/ws",
        "model": "universal-3-6-pro",
        "mode": "min_latency",
    }


def test_vendor_metadata_without_config_is_empty():
    ext = AssemblyAIASRExtension("test")
    ext.config = None

    assert ext.vendor_metadata() == {}


# --------------------------------------------------------------------------
# Turn -> asr_result mapping
# --------------------------------------------------------------------------


def test_final_formatted_turn_maps_text_timing_words_and_asr_info():
    ext, env, _ = _make_extension()
    ext.metadata = {"session_id": "123"}
    ext.audio_timeline.add_user_audio(5000)
    ext.sent_user_audio_duration_ms_before_last_reset = 1000

    asyncio.run(ext.on_result(_turn()))

    results = env.payloads("asr_result")
    assert len(results) == 1
    result = results[0]
    assert result["text"] == "Hello world."
    assert result["final"] is True
    assert result["start_ms"] == 2200
    assert result["duration_ms"] == 700
    assert result["language"] == "en-US"
    assert result["words"] == [
        {"word": "Hello", "start_ms": 2200, "duration_ms": 300, "stable": True},
        {
            "word": "world.",
            "start_ms": 2600,
            "duration_ms": 300,
            "stable": True,
        },
    ]
    assert result["metadata"]["session_id"] == "123"
    asr_info = result["metadata"]["asr_info"]
    assert asr_info["vendor"] == "assemblyai"
    assert asr_info["model"] == "universal-3-6-pro"
    assert asr_info["turn_order"] == 3
    assert asr_info["end_of_turn"] is True
    assert asr_info["turn_is_formatted"] is True
    assert asr_info["end_of_turn_confidence"] == 0.93


def test_unformatted_end_of_turn_is_interim_when_format_turns_enabled():
    ext, env, _ = _make_extension(format_turns=True)

    asyncio.run(ext.on_result(_turn(turn_is_formatted=False)))

    assert env.payloads("asr_result")[0]["final"] is False


def test_unformatted_end_of_turn_is_final_when_format_turns_disabled():
    ext, env, _ = _make_extension(format_turns=False)

    asyncio.run(ext.on_result(_turn(turn_is_formatted=False)))

    assert env.payloads("asr_result")[0]["final"] is True


def test_partial_turn_uses_all_words_and_marks_unstable_words():
    ext, env, _ = _make_extension()
    ext.audio_timeline.add_user_audio(5000)
    turn = _turn(end_of_turn=False, turn_is_formatted=False)
    turn["words"][1]["word_is_final"] = False

    asyncio.run(ext.on_result(turn))

    result = env.payloads("asr_result")[0]
    assert result["final"] is False
    assert result["start_ms"] == 1200
    assert result["duration_ms"] == 700
    assert [w["stable"] for w in result["words"]] == [True, False]


def test_partial_turn_exposes_end_of_turn_confidence_for_eager_inference():
    ext, env, _ = _make_extension()
    ext.audio_timeline.add_user_audio(5000)

    asyncio.run(
        ext.on_result(
            _turn(
                end_of_turn=False,
                turn_is_formatted=False,
                end_of_turn_confidence=0.87,
            )
        )
    )

    result = env.payloads("asr_result")[0]
    assert result["final"] is False
    assert result["metadata"]["asr_info"]["end_of_turn_confidence"] == 0.87


def test_detected_language_and_speaker_label_are_surfaced():
    ext, env, _ = _make_extension(language_detection=True, speaker_labels=True)
    ext.audio_timeline.add_user_audio(5000)

    asyncio.run(
        ext.on_result(
            _turn(
                language_code="es", language_confidence=0.9, speaker_label="A"
            )
        )
    )

    result = env.payloads("asr_result")[0]
    assert result["language"] == "es-ES"
    assert result["metadata"]["asr_info"]["language_code"] == "es"
    assert result["metadata"]["asr_info"]["language_confidence"] == 0.9
    assert result["metadata"]["asr_info"]["speaker_label"] == "A"


def test_empty_transcript_is_ignored():
    ext, env, _ = _make_extension()

    asyncio.run(ext.on_result(_turn(transcript="", words=[])))

    assert env.payloads("asr_result") == []


def test_final_result_after_finalize_emits_asr_finalize_end():
    ext, env, recognition = _make_extension()
    ext.finalize_id = "fin-1"

    async def scenario():
        await ext.finalize("123")
        await ext.on_result(_turn())

    asyncio.run(scenario())

    assert recognition.force_endpoints == 1
    ends = env.payloads("asr_finalize_end")
    assert len(ends) == 1
    assert ends[0]["finalize_id"] == "fin-1"


# --------------------------------------------------------------------------
# agent_context via tts_text_input
# --------------------------------------------------------------------------


def test_agent_reply_is_sent_as_agent_context_when_request_ends():
    ext, _, recognition = _make_extension()

    async def scenario():
        await ext.on_data(ext.ten_env, _tts_text_input("r1", "What is ", False))
        await ext.on_data(
            ext.ten_env, _tts_text_input("r1", "your email address?", True)
        )

    asyncio.run(scenario())

    assert recognition.updates == [
        {"agent_context": "What is your email address?"}
    ]


def test_new_request_id_flushes_the_previous_unfinished_reply():
    ext, _, recognition = _make_extension()

    async def scenario():
        await ext.on_data(
            ext.ten_env, _tts_text_input("r1", "Hello there.", False)
        )
        await ext.on_data(
            ext.ten_env, _tts_text_input("r2", "Second reply.", True)
        )

    asyncio.run(scenario())

    assert recognition.updates == [
        {"agent_context": "Hello there."},
        {"agent_context": "Second reply."},
    ]


def test_agent_context_is_clipped_to_the_last_1750_characters():
    ext, _, recognition = _make_extension()
    long_text = "x" * 1000 + "y" * 1000

    asyncio.run(
        ext.on_data(ext.ten_env, _tts_text_input("r1", long_text, True))
    )

    sent = recognition.updates[0]["agent_context"]
    assert len(sent) == 1750
    assert sent.endswith("y" * 1000)


def test_blank_reply_is_not_sent_as_agent_context():
    ext, _, recognition = _make_extension()

    asyncio.run(ext.on_data(ext.ten_env, _tts_text_input("r1", "  \n", True)))

    assert recognition.updates == []


def test_agent_context_is_skipped_for_universal_streaming_models():
    ext, _, recognition = _make_extension(
        speech_model="universal-streaming-english"
    )

    asyncio.run(ext.on_data(ext.ten_env, _tts_text_input("r1", "Hi.", True)))

    assert recognition.updates == []


def test_agent_context_is_deferred_until_the_session_opens():
    ext, _, recognition = _make_extension(connected=False)

    async def scenario():
        await ext.on_data(ext.ten_env, _tts_text_input("r1", "Hi there.", True))
        assert recognition.updates == []
        recognition.connected = True
        await ext.on_open("sess-1", {"model": "universal-3-5-pro"})

    asyncio.run(scenario())

    assert recognition.updates == [{"agent_context": "Hi there."}]


def test_last_agent_context_is_reseeded_after_reconnect():
    ext, _, recognition = _make_extension()

    async def scenario():
        await ext.on_data(ext.ten_env, _tts_text_input("r1", "Hi there.", True))
        await ext.on_open("sess-2", {})

    asyncio.run(scenario())

    assert recognition.updates == [
        {"agent_context": "Hi there."},
        {"agent_context": "Hi there."},
    ]


# --------------------------------------------------------------------------
# Connection status and error classification
# --------------------------------------------------------------------------


def test_on_open_marks_connected_and_resets_reconnect_counter():
    ext, env, _ = _make_extension()
    ext.reconnect_manager.attempts = 3
    ext._connection_machine.try_connecting()

    asyncio.run(ext.on_open("sess-1", {"model": "universal-3-5-pro"}))

    assert ext.connection_status == ModuleConnectionStatus.CONNECTED
    assert ext.reconnect_manager.attempts == 0
    statuses = env.payloads("connection_status_changed")
    assert statuses[-1]["current"] == "connected"


def test_auth_close_code_is_fatal_with_vendor_info_and_no_reconnect():
    ext, env, _ = _make_extension()
    ext._connection_machine.try_connecting()
    ext._connection_machine.try_connected()

    asyncio.run(ext.on_close(1008, "Not authorized"))

    errors = env.payloads("error")
    assert len(errors) == 1
    assert errors[0]["code"] == FATAL
    assert errors[0]["vendor_info"] == {
        "vendor": "assemblyai",
        "code": "1008",
        "message": "Not authorized",
    }
    assert ext.connection_status == ModuleConnectionStatus.DISCONNECTED
    ext._handle_reconnect.assert_not_awaited()


@pytest.mark.parametrize("code", [1006, 3005, 3006, 3007, 1011])
def test_transient_close_codes_are_non_fatal_and_reconnect(code):
    ext, env, _ = _make_extension()
    ext._connection_machine.try_connecting()
    ext._connection_machine.try_connected()

    asyncio.run(ext.on_close(code, "boom"))

    errors = env.payloads("error")
    assert len(errors) == 1
    assert errors[0]["code"] == NON_FATAL
    assert errors[0]["vendor_info"]["code"] == str(code)
    assert ext.connection_status == ModuleConnectionStatus.DISCONNECTED
    ext._handle_reconnect.assert_awaited_once()


def test_normal_close_reconnects_without_emitting_an_error():
    ext, env, _ = _make_extension()

    asyncio.run(ext.on_close(1000, ""))

    assert env.payloads("error") == []
    ext._handle_reconnect.assert_awaited_once()


def test_close_after_stop_does_not_reconnect():
    ext, _, _ = _make_extension()
    ext.stopped = True

    asyncio.run(ext.on_close(1006, "gone"))

    ext._handle_reconnect.assert_not_awaited()


def test_handshake_401_is_fatal_with_vendor_info_and_no_reconnect():
    ext, env, _ = _make_extension()
    ext.recognition = None

    fake = AsyncMock()
    fake.start.side_effect = AssemblyAIConnectionError(
        "401", "WebSocket handshake rejected (HTTP 401)"
    )
    with patch.object(
        extension_module, "AssemblyAIWSRecognition", return_value=fake
    ) as factory:
        asyncio.run(ext.start_connection())

    kwargs = factory.call_args.kwargs
    assert kwargs["api_key"] == "k"
    assert kwargs["config"]["speech_model"] == "universal-3-6-pro"
    assert "api_key" not in kwargs["config"]
    errors = env.payloads("error")
    assert len(errors) == 1
    assert errors[0]["code"] == FATAL
    assert errors[0]["vendor_info"]["code"] == "401"
    assert ext.connection_status == ModuleConnectionStatus.DISCONNECTED
    ext._handle_reconnect.assert_not_awaited()


def test_handshake_5xx_is_non_fatal_and_reconnects():
    ext, env, _ = _make_extension()
    ext.recognition = None

    fake = AsyncMock()
    fake.start.side_effect = AssemblyAIConnectionError("503", "unavailable")
    with patch.object(
        extension_module, "AssemblyAIWSRecognition", return_value=fake
    ):
        asyncio.run(ext.start_connection())

    errors = env.payloads("error")
    assert errors[0]["code"] == NON_FATAL
    assert errors[0]["vendor_info"]["code"] == "503"
    ext._handle_reconnect.assert_awaited_once()


def test_missing_api_key_is_fatal_and_marks_disconnected():
    ext, env, _ = _make_extension()
    ext.config.api_key = ""
    ext.recognition = None

    asyncio.run(ext.start_connection())

    errors = env.payloads("error")
    assert len(errors) == 1
    assert errors[0]["code"] == FATAL
    assert ext.connection_status == ModuleConnectionStatus.DISCONNECTED


def test_on_error_emits_non_fatal_error_with_vendor_info():
    ext, env, _ = _make_extension()

    asyncio.run(ext.on_error("Consumer loop error: x", None))

    errors = env.payloads("error")
    assert errors[0]["code"] == NON_FATAL
    assert errors[0]["vendor_info"]["vendor"] == "assemblyai"


# --------------------------------------------------------------------------
# Misc contract
# --------------------------------------------------------------------------


def test_input_sample_rate_comes_from_config():
    ext, _, _ = _make_extension(sample_rate=8000)

    assert ext.input_audio_sample_rate() == 8000


def test_on_init_loads_params_and_logs_redacted_config():
    ext = AssemblyAIASRExtension("test")
    env = _FakeTenEnv()

    class _InitEnv(_FakeTenEnv):
        async def get_property_to_json(self, _path: Optional[str] = None):
            return (
                json.dumps(
                    {
                        "params": {
                            "api_key": "top-secret",
                            "speech_model": "universal-3-5-pro",
                            "mode": "balanced",
                        }
                    }
                ),
                None,
            )

        async def get_property_bool(self, _path: str):
            return False, None

    env = _InitEnv()

    async def scenario():
        await ext.on_init(env)  # type: ignore[arg-type]
        # Let the base-class background task start, then leave it to be
        # cancelled when the loop closes.
        await asyncio.sleep(0)

    asyncio.run(scenario())

    assert ext.config.speech_model == "universal-3-5-pro"
    assert ext.config.mode == "balanced"
    config_logs = [l for l in env.logs if l.startswith("config:")]
    assert config_logs, env.logs
    assert "top-secret" not in config_logs[0]


# --------------------------------------------------------------------------
# Finalize contract: exactly one asr_finalize_end per request, bounded
# --------------------------------------------------------------------------


def test_empty_end_of_turn_after_finalize_completes_without_a_result():
    ext, env, _ = _make_extension()
    ext.finalize_id = "fin-1"

    async def scenario():
        await ext.finalize("123")
        await ext.on_result(_turn(transcript="", words=[]))

    asyncio.run(scenario())

    assert env.payloads("asr_result") == []
    assert [e["finalize_id"] for e in env.payloads("asr_finalize_end")] == [
        "fin-1"
    ]


def test_finalize_while_disconnected_completes_immediately():
    ext, env, recognition = _make_extension(connected=False)
    ext.finalize_id = "fin-2"

    asyncio.run(ext.finalize("123"))

    assert recognition.force_endpoints == 0
    assert len(env.payloads("asr_finalize_end")) == 1


def test_finalize_times_out_and_still_emits_one_finalize_end():
    ext, env, _ = _make_extension(finalize_timeout_ms=20)
    ext.finalize_id = "fin-3"

    async def scenario():
        await ext.finalize("123")
        await asyncio.sleep(0.1)
        timed_out = len(env.payloads("asr_finalize_end"))
        # A later final result must not produce a second finalize_end.
        await ext.on_result(_turn())
        return timed_out

    assert asyncio.run(scenario()) == 1
    assert len(env.payloads("asr_finalize_end")) == 1


def test_final_result_cancels_the_finalize_timeout():
    ext, env, _ = _make_extension(finalize_timeout_ms=20)
    ext.finalize_id = "fin-4"

    async def scenario():
        await ext.finalize("123")
        await ext.on_result(_turn())
        await asyncio.sleep(0.1)

    asyncio.run(scenario())

    assert len(env.payloads("asr_finalize_end")) == 1
    assert ext._finalize_timeout_task is None


# --------------------------------------------------------------------------
# Lifecycle: init latch, stale callbacks, stop races, reconnect ownership
# --------------------------------------------------------------------------


class _InitEnv(_FakeTenEnv):
    def __init__(self, params: Dict[str, Any]) -> None:
        super().__init__()
        self.params = params

    async def get_property_to_json(self, _path: Optional[str] = None):
        return json.dumps({"params": self.params}), None

    async def get_property_bool(self, _path: str):
        return False, None


def test_invalid_config_emits_one_fatal_error_and_blocks_connection():
    ext = AssemblyAIASRExtension("test")
    env = _InitEnv({"api_key": "k", "mode": "turbo"})

    async def scenario():
        await ext.on_init(env)  # type: ignore[arg-type]
        await asyncio.sleep(0)
        await ext.start_connection()

    asyncio.run(scenario())

    errors = env.payloads("error")
    assert len(errors) == 1
    assert errors[0]["code"] == FATAL
    assert "mode" in errors[0]["message"]
    assert ext.recognition is None
    assert ext.connection_status == ModuleConnectionStatus.DISCONNECTED


def test_stale_close_from_a_replaced_client_is_ignored():
    ext, env, _ = _make_extension()
    stale = ext._callback_for_epoch(ext._connection_epoch)
    ext._connection_epoch += 1

    asyncio.run(stale.on_close(3006, "old socket died"))

    assert env.payloads("error") == []
    ext._handle_reconnect.assert_not_awaited()


def test_current_epoch_callback_forwards_to_the_extension():
    ext, env, _ = _make_extension()
    current = ext._callback_for_epoch(ext._connection_epoch)

    asyncio.run(current.on_close(3006, "boom"))

    assert len(env.payloads("error")) == 1


def test_stop_during_handshake_closes_the_late_session():
    ext, env, _ = _make_extension()
    ext.recognition = None

    fake = AsyncMock()

    async def start_and_stop(*_args, **_kwargs):
        ext.stopped = True  # on_stop fired while the handshake was in flight
        return True

    fake.start.side_effect = start_and_stop
    fake.is_connected.return_value = False
    with patch.object(
        extension_module, "AssemblyAIWSRecognition", return_value=fake
    ):
        asyncio.run(ext.start_connection())

    fake.close.assert_awaited_once()
    assert ext.recognition is None
    assert env.payloads("error") == []


def test_on_open_after_stop_is_ignored():
    ext, env, recognition = _make_extension()
    ext.stopped = True
    ext._last_agent_context = "Hi."

    asyncio.run(ext.on_open("sess-1", {}))

    assert ext.connection_status != ModuleConnectionStatus.CONNECTED
    assert recognition.updates == []


def test_on_open_reports_connect_delay_metric():
    ext, env, _ = _make_extension()
    ext.recognition = None
    fake = _FakeRecognition(connected=False)

    async def scenario():
        with patch.object(
            extension_module, "AssemblyAIWSRecognition", return_value=fake
        ):
            await ext.start_connection()
        fake.connected = True
        await ext.on_open("sess-1", {"model": "universal-3-6-pro"})

    asyncio.run(scenario())

    delays = [
        m["metrics"]["connect_delay"]
        for m in env.payloads("metrics")
        if "connect_delay" in m["metrics"]
    ]
    assert len(delays) == 1
    assert delays[0] >= 0


def test_connection_status_event_masks_the_api_key():
    ext, env, _ = _make_extension(api_key="super-secret-key")
    ext._connection_machine.try_connecting()

    asyncio.run(ext.on_open("sess-1", {}))

    statuses = env.payloads("connection_status_changed")
    vendor_metadata = statuses[-1]["metadata"]["vendor_metadata"]
    assert vendor_metadata["model"] == "universal-3-6-pro"
    assert vendor_metadata["key"] != "super-secret-key"
    assert "super-secret-key" not in json.dumps(statuses)


def test_reconnect_runs_as_a_background_task_and_is_cancelled_on_stop():
    ext, _, _ = _make_extension(mock_reconnect=False)
    ext.reconnect_manager.base_delay = 10.0  # keep the task sleeping

    async def scenario():
        await ext.on_close(3006, "boom")
        task = ext._reconnect_task
        assert task is not None and not task.done()
        ext.stopped = True
        await ext.stop_connection()
        await asyncio.sleep(0)
        return task.done()

    assert asyncio.run(scenario()) is True


def test_second_close_while_a_reconnect_is_pending_does_not_start_another():
    ext, _, _ = _make_extension(mock_reconnect=False)
    ext.reconnect_manager.base_delay = 10.0

    async def scenario():
        await ext.on_close(3006, "boom")
        first = ext._reconnect_task
        await ext.on_close(1006, "again")
        same = ext._reconnect_task is first
        first.cancel()
        return same

    assert asyncio.run(scenario()) is True


def test_reconnect_ceiling_emits_one_fatal_and_latches():
    ext, env, _ = _make_extension(mock_reconnect=False)
    ext.reconnect_manager.attempts = ext.reconnect_manager.max_attempts

    async def scenario():
        await ext.on_close(3006, "boom")
        await ext.on_close(3006, "boom again")

    asyncio.run(scenario())

    fatal = [e for e in env.payloads("error") if e["code"] == FATAL]
    assert len(fatal) == 1
    assert ext._reconnect_task is None
    assert ext._fatal_latched is True


def test_dropped_legacy_params_are_logged_once_at_connect():
    ext, env, _ = _make_extension(end_of_turn_confidence_threshold=0.4)
    ext.recognition = None
    fake = _FakeRecognition(connected=False)

    with patch.object(
        extension_module, "AssemblyAIWSRecognition", return_value=fake
    ):
        asyncio.run(ext.start_connection())

    warnings = [
        l
        for l in env.logs
        if "end_of_turn_confidence_threshold" in l and "ignored" in l
    ]
    assert len(warnings) == 1
