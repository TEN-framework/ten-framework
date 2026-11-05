#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""BlazeTTSClient against an in-process Blaze realtime WebSocket."""

import asyncio
from unittest.mock import MagicMock

import pytest

from blaze_tts_python.blaze_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    BlazeTTSClient,
    BlazeTTSConnectionException,
)
from blaze_tts_python.config import BlazeTTSConfig
from blaze_tts_python.tests.mock import MockBlazeTTSServer


def make_client(server: MockBlazeTTSServer, **params) -> BlazeTTSClient:
    config = BlazeTTSConfig(
        params={"base_url": server.base_url, "api_key": "test-key", **params}
    )
    config.update_params()
    return BlazeTTSClient(config=config, ten_env=MagicMock())


async def collect(client: BlazeTTSClient, text: str) -> list[tuple]:
    return [item async for item in client.get(text)]


def audio_of(events: list[tuple]) -> bytes:
    return b"".join(d for d, e in events if e == EVENT_TTS_RESPONSE)


def test_streams_pcm_with_ttfb_then_end():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server, sample_rate=16000)
            await client.start()
            events = await collect(client, "xin chào")
            await client.stop()

        kinds = [e for _, e in events]
        assert kinds[0] == EVENT_TTS_TTFB_METRIC
        assert kinds[-1] == EVENT_TTS_END
        assert audio_of(events) == server.audio
        query = server.queries[0]
        assert query["query"] == "xin chào"
        assert query["audio_format"] == "pcm"
        assert query["sample_rate"] == 16000
        assert query["model"] == "2.0-realtime"
        assert query["speaker_id"] == "HN-Nu-CSKH-HuongGiang"
        assert "api_key" not in query and "token" not in query

    asyncio.run(_run())


def test_one_socket_serves_sequential_requests():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server)
            await client.start()
            first = await collect(client, "một")
            second = await collect(client, "hai")
            await client.stop()
            assert server.connections == 1

        assert audio_of(first) == audio_of(second) == server.audio

    asyncio.run(_run())


def test_odd_sized_chunks_keep_samples_whole():
    async def _run():
        async with MockBlazeTTSServer(chunk_size=333) as server:
            client = make_client(server)
            events = await collect(client, "xin chào")
            await client.stop()

        chunks = [d for d, e in events if e == EVENT_TTS_RESPONSE]
        assert all(len(c) % 2 == 0 for c in chunks)
        assert b"".join(chunks) == server.audio

    asyncio.run(_run())


def test_empty_text_ends_without_query():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server)
            events = await collect(client, "   ")
            await client.stop()
            assert server.queries == []

        assert events == [(None, EVENT_TTS_END)]

    asyncio.run(_run())


def test_bad_token_raises_connection_exception():
    async def _run():
        async with MockBlazeTTSServer(valid_token="other") as server:
            client = make_client(server)
            with pytest.raises(BlazeTTSConnectionException) as exc:
                await client.start()
            assert exc.value.status_code == 401

    asyncio.run(_run())


def test_failed_request_yields_error_and_next_request_reconnects():
    async def respond(query, ws, server):
        if query["query"] == "bad":
            await server.send(ws, {"type": "processing-request"})
            await server.send(ws, {"status": "started-byte-stream"})
            await server.send(
                ws,
                {
                    "status": "failed-request",
                    "details": "The TTS service is temporarily unavailable.",
                },
            )
            return
        await server.stream(ws)

    async def _run():
        async with MockBlazeTTSServer(respond=respond) as server:
            client = make_client(server)
            failed = await collect(client, "bad")
            ok = await collect(client, "good")
            await client.stop()
            assert server.connections == 2

        assert failed[-1][1] == EVENT_TTS_ERROR
        assert b"temporarily unavailable" in failed[-1][0]
        assert audio_of(ok) == server.audio

    asyncio.run(_run())


def test_cancel_mid_stream_drops_socket_and_next_request_works():
    async def respond(_query, ws, server):
        await server.send(ws, {"status": "started-byte-stream"})
        for _ in range(200):
            await ws.send(b"\x00\x00" * 480)
            await asyncio.sleep(0.005)
        await server.send(ws, {"type": "finished-byte-stream"})

    async def _run():
        async with MockBlazeTTSServer(respond=respond) as server:
            client = make_client(server)
            seen = []
            async for item in client.get("long text"):
                seen.append(item)
                if len(seen) == 3:
                    await client.cancel()
            assert EVENT_TTS_END not in [e for _, e in seen]

            server.respond = None
            after = await collect(client, "next")
            await client.stop()
            assert server.connections == 2

        assert audio_of(after) == server.audio

    asyncio.run(_run())


def test_idle_socket_closed_by_server_is_reopened():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server)
            await client.start()
            await server.sockets[0].close()
            await asyncio.sleep(0.05)
            events = await collect(client, "xin chào")
            await client.stop()
            assert server.connections == 2

        assert events[-1][1] == EVENT_TTS_END
        assert audio_of(events) == server.audio

    asyncio.run(_run())
