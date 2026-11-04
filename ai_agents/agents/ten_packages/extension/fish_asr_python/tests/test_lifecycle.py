"""Regression coverage for connection recovery and the TEN ASR contract."""

import asyncio
import copy
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from ten_runtime import Data
from websockets.exceptions import ConnectionClosedError, ConnectionClosedOK

from .. import extension as extension_module
from ..config import FishAudioASRConfig
from ..extension import FishAudioASRExtension


class FakeClient:
    def __init__(self, **options):
        self.on_message = options["on_message"]
        self.on_close = options["on_close"]
        self.connected = False
        self.session_done = asyncio.Event()
        self.send_audio = AsyncMock()
        self.finalize = AsyncMock()
        self.close = AsyncMock(side_effect=self._close)

    async def connect(self):
        self.connected = True
        await self.on_message({"type": "session_started"})

    async def _close(self):
        self.connected = False


def make_extension(*, turn_mode="auto"):
    extension = FishAudioASRExtension("fish_asr_python")
    env = MagicMock()
    env.send_data = AsyncMock()
    extension.ten_env = env
    extension.config = FishAudioASRConfig(
        api_key="test-key", turn_mode=turn_mode
    )
    extension.metadata = {"session_id": "123"}
    extension.session_id = "123"
    return extension, env


def output(env, name):
    return [
        json.loads(call.args[0].get_property_to_json("")[0])
        for call in env.send_data.call_args_list
        if call.args[0].get_name() == name
    ]


def audio_frame(data, session_id="123"):
    return SimpleNamespace(
        get_buf=lambda: data,
        get_property_to_json=lambda _key: (
            json.dumps({"session_id": session_id}),
            None,
        ),
    )


def finalize_data(finalize_id, session_id):
    data = Data.create("asr_finalize")
    data.set_property_from_json(
        None,
        json.dumps(
            {
                "finalize_id": finalize_id,
                "metadata": {"session_id": session_id},
            }
        ),
    )
    return data


def committed(turn_index=0, **values):
    return {
        "type": "committed",
        "turn_index": turn_index,
        "text": "hello",
        "audio_start_ms": 0,
        "audio_end_ms": 20,
        "language_code": "en",
        "user_turn": True,
        "reason": "finalize",
        **values,
    }


def test_transient_connect_failures_retry_with_backoff_until_ready():
    async def run():
        extension, env = make_extension()
        clients = []
        delays = []
        real_sleep = asyncio.sleep

        async def fast_sleep(delay):
            delays.append(delay)
            await real_sleep(0)

        def create_client(**options):
            client = FakeClient(**options)
            clients.append(client)
            if len(clients) < 4:
                client.connect = AsyncMock(
                    side_effect=ConnectionRefusedError("temporary outage")
                )
            return client

        with (
            patch.object(extension_module, "FishAudioASRClient", create_client),
            patch.object(extension_module.asyncio, "sleep", fast_sleep),
        ):
            try:
                await extension.start_connection()
                recovery = extension._reconnect_task
                assert recovery is not None
                await asyncio.wait_for(recovery, timeout=1)
                assert extension.is_connected()
                assert len(clients) == 4
                assert delays == [0.5, 1.0, 2.0]
                assert extension._reconnect_task is None
                assert [event["code"] for event in output(env, "error")] == [
                    1000,
                    1000,
                    1000,
                ]
            finally:
                await extension.stop_connection()

    asyncio.run(run())


@pytest.mark.parametrize("status", [400, 401, 402, 403])
def test_terminal_handshake_refusals_do_not_retry(status):
    async def run():
        extension, env = make_extension()
        error = RuntimeError("handshake refused")
        error.response = SimpleNamespace(status_code=status)

        def create_client(**options):
            client = FakeClient(**options)
            client.connect = AsyncMock(side_effect=error)
            return client

        with patch.object(
            extension_module, "FishAudioASRClient", create_client
        ):
            await extension.start_connection()
            await extension.start_connection()
        assert extension._session_fatal
        assert extension._reconnect_task is None
        assert len(output(env, "error")) == 1
        assert output(env, "error")[0]["code"] == -1000
        assert output(env, "error")[0]["vendor_info"]["code"] == str(status)

    asyncio.run(run())


@pytest.mark.parametrize("status", [429, 503])
def test_capacity_refusals_schedule_recovery(status):
    async def run():
        extension, env = make_extension()
        error = RuntimeError("temporarily unavailable")
        error.response = SimpleNamespace(status_code=status)

        def create_client(**options):
            client = FakeClient(**options)
            client.connect = AsyncMock(side_effect=error)
            return client

        with patch.object(
            extension_module, "FishAudioASRClient", create_client
        ):
            await extension.start_connection()
            recovery = extension._reconnect_task
            assert recovery is not None
            await extension.stop_connection()
        assert recovery.done()
        assert not extension._session_fatal
        assert output(env, "error")[0]["code"] == 1000
        assert extension._reconnect_task is None

    asyncio.run(run())


def test_handshake_discards_stale_buffered_frames_at_session_boundary():
    async def run():
        extension, _env = make_extension()
        first = b"\x01\x02" * 320
        second = b"\x03\x04" * 320
        observed = []
        await extension._handle_audio_frame(
            extension.ten_env, audio_frame(first, "first-session")
        )
        await extension._handle_audio_frame(
            extension.ten_env, audio_frame(second, "second-session")
        )

        def create_client(**options):
            client = FakeClient(**options)

            async def send(data):
                observed.append((data, extension.metadata["session_id"]))

            client.send_audio.side_effect = send
            return client

        with patch.object(
            extension_module, "FishAudioASRClient", create_client
        ):
            try:
                await extension.start_connection()
                assert observed == [(second, "second-session")]
                assert extension.buffered_frames.empty()
                assert extension.buffered_frames_size == 0
                assert extension.sent_buffer_length == len(second)
                await extension._handle_message(committed(text="second"))
                assert (
                    output(_env, "asr_result")[-1]["metadata"]["session_id"]
                    == "second-session"
                )
                assert extension.first_audio_time is not None
                assert (
                    extension.audio_timeline.get_total_user_audio_duration()
                    == 40
                )
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_disconnected_finalize_discards_audio_before_reconnect():
    async def run():
        extension, env = make_extension()
        old_audio = b"old" * 320
        next_audio = b"new" * 320
        await extension._handle_audio_frame(
            env, audio_frame(old_audio, "turn-session")
        )
        assert extension.buffered_frames.qsize() == 1

        await extension.on_data(env, finalize_data("finish", "turn-session"))
        assert extension.buffered_frames.empty()
        assert output(env, "asr_finalize_end") == [
            {
                "finalize_id": "finish",
                "metadata": {"session_id": "turn-session"},
            }
        ]

        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            try:
                await extension.start_connection()
                await extension._handle_audio_frame(
                    env, audio_frame(next_audio, "next-session")
                )
                assert extension.client.send_audio.call_args.args == (
                    next_audio,
                )
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_unlisted_close_code_reconnects_with_backoff():
    async def run():
        extension, env = make_extension()
        real_sleep = asyncio.sleep

        async def fast_sleep(_delay):
            await real_sleep(0)

        with (
            patch.object(extension_module, "FishAudioASRClient", FakeClient),
            patch.object(extension_module.asyncio, "sleep", fast_sleep),
        ):
            try:
                await extension.start_connection()
                previous_client = extension.client
                previous_client.connected = False
                await previous_client.on_close(1014, "bad gateway")
                recovery = extension._reconnect_task
                assert recovery is not None
                await asyncio.wait_for(recovery, timeout=1)
                assert extension.is_connected()
                assert extension.client is not previous_client
                assert output(env, "error")[0]["code"] == 1000
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_close_exception_type_controls_normal_close_classification():
    async def run():
        normal_extension, normal_env = make_extension()
        await normal_extension._handle_close(
            1000,
            "complete",
            close_exception=ConnectionClosedOK(None, None),
        )
        assert output(normal_env, "error") == []
        await normal_extension.stop_connection()

        error_extension, error_env = make_extension()
        await error_extension._handle_close(
            1000,
            "incomplete",
            close_exception=ConnectionClosedError(None, None),
        )
        assert output(error_env, "error")[0]["code"] == 1000
        await error_extension.stop_connection()

    asyncio.run(run())


def test_vendor_metadata_redacts_signed_url():
    extension, _env = make_extension()
    extension.config.url = (
        "wss://url-user:url-password@example.test/realtime?"
        "token=signed-token&signature=signature-value"
    )
    metadata = extension.vendor_metadata()
    assert "url-user" not in metadata["url"]
    assert "url-password" not in metadata["url"]
    assert "signed-token" not in metadata["url"]
    assert "signature-value" not in metadata["url"]


def test_live_frame_cannot_duplicate_the_handshake_drain():
    async def run():
        extension, env = make_extension()
        saved = b"\x01\x02" * 320
        live = b"\x03\x04" * 320
        sending = asyncio.Event()
        release = asyncio.Event()
        sent = []
        await extension._handle_audio_frame(env, audio_frame(saved))

        def create_client(**options):
            client = FakeClient(**options)

            async def send(data):
                sent.append(data)
                if data == saved:
                    sending.set()
                    await release.wait()

            client.send_audio.side_effect = send
            return client

        with patch.object(
            extension_module, "FishAudioASRClient", create_client
        ):
            connection = asyncio.create_task(extension.start_connection())
            live_send = None
            try:
                await asyncio.wait_for(sending.wait(), timeout=1)
                live_send = asyncio.create_task(
                    extension._handle_audio_frame(env, audio_frame(live))
                )
                release.set()
                await asyncio.wait_for(connection, timeout=1)
                await asyncio.wait_for(live_send, timeout=1)
                assert sent == [saved, live]
                assert extension.buffered_frames_size == 0
            finally:
                release.set()
                await asyncio.gather(
                    connection,
                    *([live_send] if live_send else []),
                    return_exceptions=True,
                )
                await extension.stop_connection()

    asyncio.run(run())


def test_failed_buffer_drain_preserves_unsent_frames_in_order():
    async def run():
        extension, env = make_extension()
        first = audio_frame(b"\x01\x02" * 320)
        second = audio_frame(b"\x03\x04" * 320)
        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            try:
                await extension.start_connection()
                extension.client.connected = False
                await extension._handle_audio_frame(env, first)
                await extension._handle_audio_frame(env, second)
                extension.client.connected = True
                extension.client.send_audio.side_effect = OSError("offline")
                async with extension._audio_lock:
                    assert not await extension._flush_buffered_frames_locked()
                assert extension.buffered_frames_size == 1280
                assert extension.buffered_frames.get_nowait() is first
                assert extension.buffered_frames.get_nowait() is second
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_concurrent_finalizes_echo_independent_ids_and_sessions():
    async def run():
        extension, env = make_extension()
        sent = asyncio.Queue()
        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            await extension.start_connection()
            extension.client.finalize.side_effect = lambda: sent.put_nowait(
                True
            )
            first = asyncio.create_task(
                extension.on_data(env, finalize_data("first", "session-a"))
            )
            second = None
            try:
                await asyncio.wait_for(sent.get(), timeout=1)
                second = asyncio.create_task(
                    extension.on_data(env, finalize_data("second", "session-b"))
                )
                await extension._handle_audio_frame(
                    env, audio_frame(b"\0" * 640, "later-audio-session")
                )
                await extension._handle_message(committed(reason="eot"))
                assert output(env, "asr_finalize_end") == []
                await extension._handle_message(committed(1))
                await asyncio.wait_for(first, timeout=1)
                await asyncio.wait_for(sent.get(), timeout=1)
                await extension._handle_message(committed(2))
                await asyncio.wait_for(second, timeout=1)
                assert output(env, "asr_finalize_end") == [
                    {
                        "finalize_id": "first",
                        "metadata": {"session_id": "session-a"},
                    },
                    {
                        "finalize_id": "second",
                        "metadata": {"session_id": "session-b"},
                    },
                ]
                names = [
                    call.args[0].get_name()
                    for call in env.send_data.call_args_list
                ]
                final_result = max(
                    i for i, name in enumerate(names) if name == "asr_result"
                )
                assert final_result < len(names) - 1
                assert names[-1] == "asr_finalize_end"
            finally:
                for task in [first, second]:
                    if task is not None and not task.done():
                        task.cancel()
                        await asyncio.gather(task, return_exceptions=True)
                await extension.stop_connection()

    asyncio.run(run())


def test_disconnected_finalize_echoes_request_metadata():
    async def run():
        extension, env = make_extension()
        await extension.on_data(env, finalize_data("finish", "new-session"))
        assert output(env, "asr_finalize_end") == [
            {
                "finalize_id": "finish",
                "metadata": {"session_id": "new-session"},
            }
        ]
        assert extension.metadata == {"session_id": "123"}

    asyncio.run(run())


@pytest.mark.parametrize("failure", ["timeout", "send"])
def test_finalize_failures_still_acknowledge_once(failure):
    async def run():
        extension, env = make_extension()
        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            try:
                await extension.start_connection()
                if failure == "send":
                    extension.client.finalize.side_effect = OSError("closed")
                    await extension.on_data(
                        env, finalize_data("finish", "session-a")
                    )
                else:
                    with patch.object(
                        extension_module.asyncio,
                        "wait_for",
                        new=AsyncMock(side_effect=asyncio.TimeoutError),
                    ):
                        await extension.on_data(
                            env, finalize_data("finish", "session-a")
                        )
                assert output(env, "asr_finalize_end") == [
                    {
                        "finalize_id": "finish",
                        "metadata": {"session_id": "session-a"},
                    }
                ]
                assert extension._finalize_waiter is None
                assert extension.last_finalize_time is None
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_sentence_and_word_timestamps_continue_after_reconnect():
    async def run():
        extension, env = make_extension()
        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            try:
                await extension.start_connection()
                assert await extension.send_audio(
                    audio_frame(b"\0" * 160000), "123"
                )
                extension.client.connected = False
                await extension.start_connection()
                assert await extension.send_audio(
                    audio_frame(b"\0" * 32000), "123"
                )
                await extension._handle_message(
                    committed(
                        audio_start_ms=100,
                        audio_end_ms=800,
                        words=[
                            {"text": "hello", "start_ms": 100, "end_ms": 500}
                        ],
                    )
                )
                result = output(env, "asr_result")[-1]
                assert result["start_ms"] == 5100
                assert result["duration_ms"] == 700
                assert result["words"] == [
                    {
                        "word": "hello",
                        "start_ms": 5100,
                        "duration_ms": 400,
                        "stable": True,
                    }
                ]
                assert (
                    extension.audio_timeline.get_total_user_audio_duration()
                    == 6000
                )
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_auto_drafts_wait_for_user_turn_and_replay_the_latest_draft():
    async def run():
        extension, env = make_extension()
        await extension._handle_message(
            {"type": "draft", "turn_index": 0, "text": "background TV"}
        )
        await extension._handle_message(
            committed(0, text="background TV", user_turn=False)
        )
        assert output(env, "asr_result") == []
        await extension._handle_message(
            {"type": "draft", "turn_index": 1, "text": "first draft"}
        )
        await extension._handle_message(
            {"type": "draft", "turn_index": 1, "text": "latest draft"}
        )
        assert output(env, "asr_result") == []
        await extension._handle_message(
            {"type": "user_turn_taken", "turn_index": 1}
        )
        assert [r["text"] for r in output(env, "asr_result")] == [
            "latest draft"
        ]
        await extension._handle_message(committed(1, text="final text"))
        assert [r["final"] for r in output(env, "asr_result")] == [False, True]

    asyncio.run(run())


@pytest.mark.parametrize("turn_mode", ["manual", "vad"])
def test_non_auto_drafts_do_not_require_user_turn_taken(turn_mode):
    async def run():
        extension, env = make_extension(turn_mode=turn_mode)
        await extension._handle_message(
            {"type": "draft", "turn_index": 0, "text": "hello"}
        )
        assert output(env, "asr_result")[0]["text"] == "hello"

    asyncio.run(run())


def test_result_metadata_is_nested_without_mutating_input_metadata():
    async def run():
        extension, env = make_extension()
        original = {
            "session_id": "123",
            "custom": "preserved",
            "asr_info": {"vendor": "input", "turn_index": 99},
        }
        snapshot = copy.deepcopy(original)
        extension.metadata = original
        await extension._handle_message(committed(2, reason="eot"))
        result = output(env, "asr_result")[0]
        assert result["metadata"] == {
            "session_id": "123",
            "custom": "preserved",
            "asr_info": {
                "vendor": "fish_audio",
                "turn_index": 2,
                "user_turn": True,
                "turn_kind": "user",
                "reason": "eot",
                "degraded": False,
            },
        }
        assert original == snapshot
        assert extension.metadata is original

    asyncio.run(run())


def test_session_expiry_reconnects_without_a_fatal_error():
    async def run():
        extension, env = make_extension()
        real_sleep = asyncio.sleep

        async def fast_sleep(_delay):
            await real_sleep(0)

        with (
            patch.object(extension_module, "FishAudioASRClient", FakeClient),
            patch.object(extension_module.asyncio, "sleep", fast_sleep),
        ):
            try:
                await extension.start_connection()
                previous_client = extension.client
                await previous_client.on_message(
                    {
                        "type": "error",
                        "error_type": "max_duration_exceeded",
                        "code": 413,
                        "message": "session expired",
                        "recoverable": False,
                    }
                )
                previous_client.connected = False
                await previous_client.on_close(4413, "session expired")
                recovery = extension._reconnect_task
                assert recovery is not None
                await asyncio.wait_for(recovery, timeout=1)
                assert extension.is_connected()
                assert extension.client is not previous_client
                assert not extension._session_fatal
                assert [e["code"] for e in output(env, "error")] == [1000]
            finally:
                await extension.stop_connection()

    asyncio.run(run())


def test_old_client_callbacks_cannot_change_a_replacement_connection():
    async def run():
        extension, env = make_extension()
        with patch.object(extension_module, "FishAudioASRClient", FakeClient):
            try:
                await extension.start_connection()
                previous_client = extension.client
                previous_client.connected = False
                await extension.start_connection()
                current_client = extension.client
                env.send_data.reset_mock()
                await previous_client.on_message(
                    {
                        "type": "error",
                        "error_type": "invalid_config",
                        "recoverable": False,
                    }
                )
                await previous_client.on_close(1006, "late close")
                assert extension.client is current_client
                assert extension.is_connected()
                assert not extension._session_fatal
                assert extension._reconnect_task is None
                assert env.send_data.call_count == 0
            finally:
                await extension.stop_connection()

    asyncio.run(run())
