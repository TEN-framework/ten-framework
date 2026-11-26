#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Cancellation racing socket opens and newer requests (real client)."""

import asyncio
import json
from unittest.mock import MagicMock, patch

import websockets

from blaze_tts_python.blaze_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    BlazeTTSClient,
)
from blaze_tts_python.config import BlazeTTSConfig
from blaze_tts_python.tests.mock import (
    AUDIO_CHUNK,
    BASIC_CONFIG,
    MockBlazeTTSServer,
    RecordingTester,
    install_mock_client,
)

OLD_AUDIO = b"\x0a\x0a" * 480
NEW_AUDIO = b"\x0b\x0b" * 2400


def make_client(server: MockBlazeTTSServer) -> BlazeTTSClient:
    config = BlazeTTSConfig(
        params={"base_url": server.base_url, "api_key": "test-key"}
    )
    config.update_params()
    return BlazeTTSClient(config=config, ten_env=MagicMock())


async def collect(client: BlazeTTSClient, text: str) -> list[tuple]:
    return [item async for item in client.get(text)]


def audio_of(events: list[tuple]) -> bytes:
    return b"".join(d for d, e in events if e == EVENT_TTS_RESPONSE)


class GatedConnect:
    """Holds websockets.connect() open until released."""

    def __init__(self):
        self.real = websockets.connect
        self.entered = asyncio.Event()
        self.release = asyncio.Event()

    async def __call__(self, *args, **kwargs):
        self.entered.set()
        await self.release.wait()
        return await self.real(*args, **kwargs)


def test_cancel_while_the_socket_is_opening_sends_nothing():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server)  # no preheat: get() must connect
            gate = GatedConnect()
            with patch("blaze_tts_python.blaze_tts.websockets.connect", gate):
                request = asyncio.create_task(collect(client, "bị ngắt"))
                await gate.entered.wait()
                await client.cancel()
                gate.release.set()
                events = await asyncio.wait_for(request, 5)

            assert events == []
            await asyncio.sleep(0.05)
            assert server.queries == []

            # The next request still works.
            after = await collect(client, "tiếp theo")
            assert audio_of(after) == server.audio
            assert [q["query"] for q in server.queries] == ["tiếp theo"]
            await client.stop()

    asyncio.run(_run())


def test_cancel_while_reconnecting_a_closed_socket_sends_nothing():
    async def _run():
        async with MockBlazeTTSServer() as server:
            client = make_client(server)
            await client.start()
            # The server drops the idle socket; the client still holds it.
            await server.sockets[0].close()
            await asyncio.sleep(0.05)

            gate = GatedConnect()
            with patch("blaze_tts_python.blaze_tts.websockets.connect", gate):
                request = asyncio.create_task(collect(client, "bị ngắt"))
                await gate.entered.wait()
                await client.cancel()
                gate.release.set()
                events = await asyncio.wait_for(request, 5)

            assert events == []
            await asyncio.sleep(0.05)
            assert server.queries == []
            after = await collect(client, "tiếp theo")
            assert audio_of(after) == server.audio
            await client.stop()

    asyncio.run(_run())


def test_cancelled_stream_cannot_close_the_next_requests_socket():
    async def respond(query, ws, server):
        if query["query"] == "cũ":
            await server.send(ws, {"status": "started-byte-stream"})
            try:
                for _ in range(40):
                    await ws.send(OLD_AUDIO)
                    await asyncio.sleep(0.02)
            except websockets.exceptions.ConnectionClosed:
                return
            await server.send(ws, {"type": "finished-byte-stream"})
        else:
            await server.stream(ws, NEW_AUDIO)

    async def _run():
        async with MockBlazeTTSServer(respond=respond) as server:
            client = make_client(server)
            await client.start()

            old_events: list[tuple] = []
            first_audio = asyncio.Event()

            async def consume_old():
                async for item in client.get("cũ"):
                    old_events.append(item)
                    if item[1] == EVENT_TTS_RESPONSE:
                        first_audio.set()

            old = asyncio.create_task(consume_old())
            await first_audio.wait()
            # Flush and the next request overlap.
            _, new_events = await asyncio.gather(
                client.cancel(), collect(client, "mới")
            )
            await asyncio.wait_for(old, 5)

            # The old request ends quietly with only its own audio.
            assert set(audio_of(old_events)) <= {0x0A}
            assert not any(
                e in (EVENT_TTS_END, EVENT_TTS_ERROR) for _, e in old_events
            )
            # The new request gets all of its audio, none of the old.
            assert audio_of(new_events) == NEW_AUDIO
            assert new_events[-1][1] == EVENT_TTS_END

            # The new socket survived the old stream's close: a third
            # request reuses it instead of reconnecting.
            assert server.connections == 2
            third = await collect(client, "ba")
            assert audio_of(third) == NEW_AUDIO
            assert server.connections == 2
            assert [q["query"] for q in server.queries] == ["cũ", "mới", "ba"]
            await client.stop()

    asyncio.run(_run())


def test_interleaved_request_ids_stay_separate():
    texts_seen: list[str] = []
    with patch("blaze_tts_python.extension.BlazeTTSClient") as client_cls:
        install_mock_client(client_cls, texts_seen=texts_seen, chunks=2)
        tester = RecordingTester(
            [
                ("req-1", "một", False),
                ("req-2", "hai", True),
                ("req-1", "ba", True),
            ],
            expected_ends=2,
        )
        tester.set_test_mode_single(
            "blaze_tts_python", json.dumps(BASIC_CONFIG)
        )
        tester.run()

    # req-2 waits until req-1 has finished, then runs on its own.
    assert texts_seen == ["một", "ba", "hai"]
    order = [
        (name, payload.get("request_id"))
        for name, payload in tester.events
        if name in ("tts_audio_start", "tts_audio_end")
    ]
    assert order == [
        ("tts_audio_start", "req-1"),
        ("tts_audio_end", "req-1"),
        ("tts_audio_start", "req-2"),
        ("tts_audio_end", "req-2"),
    ]
    ends = {p["request_id"]: p for p in tester.payloads("tts_audio_end")}
    chunk_ms = len(AUDIO_CHUNK) * 1000 // (24000 * 2)
    assert ends["req-1"]["request_total_audio_duration_ms"] == 4 * chunk_ms
    assert ends["req-2"]["request_total_audio_duration_ms"] == 2 * chunk_ms
    assert tester.audio_bytes == 6 * len(AUDIO_CHUNK)
