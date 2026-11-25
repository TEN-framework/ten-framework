#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from datetime import datetime
from unittest.mock import AsyncMock, MagicMock, patch

import pytest
from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from deepgram_tts.config import DeepgramTTSConfig
from deepgram_tts.deepgram_tts import (
    DeepgramTTSClient,
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)


def test_per_sentence_flush_defaults_off():
    assert DeepgramTTSConfig().per_sentence_flush is False
    config = DeepgramTTSConfig(params={"per_sentence_flush": True})
    config.update_params()
    assert config.per_sentence_flush
    assert "per_sentence_flush" not in config.params


def test_per_sentence_flush_rejects_non_boolean_value():
    config = DeepgramTTSConfig(params={"per_sentence_flush": "false"})

    with pytest.raises(ValueError, match="must be a boolean"):
        config.update_params()


def test_fragments_flush_only_at_request_end():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.close = AsyncMock()
        responses = iter([b"first", b"second", '{"type":"Flushed"}'])

        async def recv():
            return next(responses)

        ws.recv.side_effect = recv
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        first = [event async for event in client.get("Hello ", flush=False)]
        space = [event async for event in client.get(" ", flush=False)]
        second = [event async for event in client.get("world.", flush=False)]
        last = [event async for event in client.get("", flush=True)]

        assert first == []
        assert space == []
        assert second == []
        assert last[0][1] == EVENT_TTS_TTFB_METRIC
        assert last[1:] == [
            (b"first", EVENT_TTS_RESPONSE),
            (b"second", EVENT_TTS_RESPONSE),
            (None, EVENT_TTS_END),
        ]
        assert [
            json.loads(call.args[0]) for call in ws.send.await_args_list
        ] == [
            {"type": "Speak", "text": "Hello "},
            {"type": "Speak", "text": " "},
            {"type": "Speak", "text": "world."},
            {"type": "Flush"},
        ]

    asyncio.run(run())


def test_cancel_clears_buffered_text():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.close = AsyncMock()
        ws.recv = AsyncMock(return_value='{"type":"Cleared"}')
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws
        client._pending_text = True

        await client.cancel()

        ws.close.assert_awaited_once()
        ws.send.assert_not_awaited()
        assert client._pending_text is False
        assert client._needs_reconnect is True

    asyncio.run(run())


def test_flux_flush_keeps_each_turns_audio_on_the_same_connection():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock(
            side_effect=[
                '{"type":"Flushed"}',
                b"first",
                '{"type":"SpeechMetadata"}',
                b"second",
                '{"type":"Flushed"}',
                '{"type":"SpeechMetadata"}',
            ]
        )
        config = DeepgramTTSConfig(
            base_url="wss://api.deepgram.com/v2/speak",
            model="flux-alexis-en",
        )
        client = DeepgramTTSClient(config, MagicMock())
        client._ws = ws

        for text, audio in (
            ("First turn.", b"first"),
            ("Second turn.", b"second"),
        ):
            client.reset_ttfb()
            events = [
                event
                async for event in client.get(text)
                if event[1] != EVENT_TTS_TTFB_METRIC
            ]
            assert events == [
                (audio, EVENT_TTS_RESPONSE),
                (None, EVENT_TTS_END),
            ]

    asyncio.run(run())


def test_cancel_ignores_receive_failure_after_socket_close():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.close = AsyncMock()
        blocked = asyncio.Event()
        closed = asyncio.Event()
        ws.close.side_effect = closed.set

        async def receive():
            blocked.set()
            await closed.wait()
            raise RuntimeError("connection closed")

        ws.recv = receive
        client = DeepgramTTSClient(
            DeepgramTTSConfig(base_url="wss://api.deepgram.com/v2/speak"),
            MagicMock(),
        )
        client._ws = ws
        client._pending_text = True
        task = asyncio.create_task(client.get("cancel me").__anext__())
        await blocked.wait()
        await client.cancel()
        with pytest.raises(StopAsyncIteration):
            await task

    asyncio.run(run())


def test_cancel_ignores_send_failure_after_socket_close():
    async def run():
        ws = MagicMock()
        started = asyncio.Event()
        closed = asyncio.Event()

        async def send(_message):
            started.set()
            await closed.wait()
            raise RuntimeError("connection closed")

        async def close():
            closed.set()

        ws.send = send
        ws.close = close
        client = DeepgramTTSClient(
            DeepgramTTSConfig(base_url="wss://api.deepgram.com/v2/speak"),
            MagicMock(),
        )
        client._ws = ws
        client._pending_text = True
        task = asyncio.create_task(client.get("cancel me").__anext__())
        await started.wait()
        await client.cancel()
        with pytest.raises(StopAsyncIteration):
            await task

    asyncio.run(run())


def test_cancel_without_buffered_text_does_not_touch_websocket():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock()
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        await client.cancel()

        ws.send.assert_not_awaited()
        ws.recv.assert_not_awaited()
        assert client._needs_reconnect is False

    asyncio.run(run())


def test_discard_pending_clears_buffered_text():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock(return_value='{"type":"Cleared"}')
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws
        client._pending_text = True

        await client.discard_pending()

        ws.send.assert_awaited_once_with(json.dumps({"type": "Clear"}))
        assert client._pending_text is False

    asyncio.run(run())


def test_reconnect_discards_pending_text_before_empty_final():
    async def run():
        old_ws = MagicMock()
        old_ws.close = AsyncMock()
        new_ws = MagicMock()
        new_ws.send = AsyncMock()
        ten_env = MagicMock()
        client = DeepgramTTSClient(DeepgramTTSConfig(), ten_env)
        client._ws = old_ws
        client._pending_text = True
        client._needs_reconnect = True

        with patch(
            "deepgram_tts.deepgram_tts.websockets.connect",
            new=AsyncMock(return_value=new_ws),
        ):
            events = [event async for event in client.get("", flush=True)]

        assert events == [(None, EVENT_TTS_END)]
        new_ws.send.assert_not_awaited()
        ten_env.log_warn.assert_called_once()

    asyncio.run(run())


def test_non_final_fragments_keep_first_ttfb_timestamp():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws
        first_sent_at = datetime(2026, 1, 1)
        client._sent_ts = first_sent_at

        events = [event async for event in client.get("next", flush=False)]

        assert events == []
        assert client._sent_ts is first_sent_at

    asyncio.run(run())


def test_send_failure_marks_connection_for_reconnect():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock(side_effect=ConnectionError("socket closed"))
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        events = [event async for event in client.get("text", flush=False)]

        assert events[0][1] == EVENT_TTS_ERROR
        assert client._needs_reconnect is True
        assert client._pending_text is False

    asyncio.run(run())


def test_vendor_error_clears_pending_text():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock(return_value='{"type":"Error","err_msg":"failed"}')
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        events = [event async for event in client.get("text")]

        assert events[0][1] == EVENT_TTS_ERROR
        assert client._needs_reconnect is True
        assert client._pending_text is False

    asyncio.run(run())


def test_explicit_fragment_flush():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock(return_value='{"type":"Flushed"}')
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        events = [event async for event in client.get("Hello.")]

        assert events == [(None, EVENT_TTS_END)]
        assert [
            json.loads(call.args[0]) for call in ws.send.await_args_list
        ] == [
            {"type": "Speak", "text": "Hello."},
            {"type": "Flush"},
        ]

    asyncio.run(run())


class ExtensionTesterBatchedEmptyFinal(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_count = 0
        self.audio_end_count = 0
        self.audio_frame_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for text, text_input_end in (
            ("Hello ", False),
            (" ", False),
            ("world.", False),
            ("", True),
        ):
            tts_input = TTSTextInput(
                request_id="batched_request",
                text=text,
                text_input_end=text_input_end,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        if data.get_name() == "tts_audio_start":
            self.audio_start_count += 1
        elif data.get_name() == "tts_audio_end":
            self.audio_end_count += 1
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame) -> None:
        self.audio_frame_count += 1


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_extension_batches_fragments_until_empty_final(MockDeepgramTTSClient):
    calls = []
    mock_client = MockDeepgramTTSClient.return_value
    mock_client.start = AsyncMock()
    mock_client.stop = AsyncMock()
    mock_client.cancel = AsyncMock()
    mock_client.reset_ttfb = MagicMock()

    async def mock_get(text: str, flush: bool = True):
        calls.append((text, flush))
        if flush:
            yield (100, EVENT_TTS_TTFB_METRIC)
            yield (b"\x00\x01" * 200, EVENT_TTS_RESPONSE)
            yield (None, EVENT_TTS_END)

    mock_client.get.side_effect = mock_get

    tester = ExtensionTesterBatchedEmptyFinal()
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "per_sentence_flush": False,
                }
            }
        ),
    )
    tester.run()

    assert calls == [
        ("Hello ", False),
        (" ", False),
        ("world.", False),
        ("", True),
    ]
    assert tester.audio_start_count == 1
    assert tester.audio_end_count == 1
    assert tester.audio_frame_count > 0


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_extension_flushes_each_fragment_when_enabled(MockDeepgramTTSClient):
    calls = []
    mock_client = MockDeepgramTTSClient.return_value
    mock_client.start = AsyncMock()
    mock_client.stop = AsyncMock()
    mock_client.cancel = AsyncMock()
    mock_client.reset_ttfb = MagicMock()

    async def mock_get(text: str):
        calls.append(text)
        if len(calls) == 1:
            yield (100, EVENT_TTS_TTFB_METRIC)
        yield (b"\x00\x01" * 200, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock_client.get.side_effect = mock_get

    tester = ExtensionTesterBatchedEmptyFinal()
    tester.set_test_mode_single(
        "deepgram_tts",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "per_sentence_flush": True,
                }
            }
        ),
    )
    tester.run()

    assert calls == ["Hello", "world."]
    assert tester.audio_start_count == 1
    assert tester.audio_end_count == 1
