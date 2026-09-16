import asyncio
import json
from datetime import datetime
from unittest.mock import AsyncMock, MagicMock, patch

from deepgram_tts.config import DeepgramTTSConfig
from deepgram_tts.deepgram_tts import (
    DeepgramTTSClient,
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)


def test_per_sentence_flush_defaults_off():
    assert DeepgramTTSConfig().per_sentence_flush is False
    config = DeepgramTTSConfig(params={"per_sentence_flush": True})
    config.update_params()
    assert config.per_sentence_flush
    assert "per_sentence_flush" not in config.params


def test_fragments_flush_only_at_request_end():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        responses = iter([b"first", b"second", '{"type":"Flushed"}'])

        async def recv():
            return next(responses)

        ws.recv.side_effect = recv
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws

        first = [event async for event in client.get("Hello ", flush=False)]
        second = [event async for event in client.get("world.", flush=False)]
        last = [event async for event in client.get("", flush=True)]

        assert first == []
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
            {"type": "Speak", "text": "world."},
            {"type": "Flush"},
        ]

    asyncio.run(run())


def test_cancel_clears_buffered_text():
    async def run():
        ws = MagicMock()
        ws.send = AsyncMock()
        ws.recv = AsyncMock(return_value='{"type":"Cleared"}')
        client = DeepgramTTSClient(DeepgramTTSConfig(), MagicMock())
        client._ws = ws
        client._pending_text = True

        await client.cancel()

        ws.send.assert_awaited_once_with(json.dumps({"type": "Clear"}))
        assert client._pending_text is False
        assert client._needs_reconnect is False

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
