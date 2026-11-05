#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from typing import Any, Callable
from unittest.mock import AsyncMock, MagicMock

import websockets

from ten_runtime import Data, ExtensionTester, TenEnvTester
from ten_ai_base.struct import TTSTextInput

from blaze_tts_python.blaze_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
)

BASIC_CONFIG = {
    "params": {
        "base_url": "http://blaze.test",
        "api_key": "test_api_key",
        "speaker_id": "speaker-1",
        "sample_rate": 24000,
    },
}

# 960 bytes = 20 ms of 24 kHz PCM16 mono.
AUDIO_CHUNK = b"\x11\x22" * 480


# ---------------------------------------------------------------------------
# Extension-level helpers: replace BlazeTTSClient with a scripted mock.
# ---------------------------------------------------------------------------


def install_mock_client(
    mock_client_cls: MagicMock,
    texts_seen: list[str] | None = None,
    chunks: int = 1,
    delay: float = 0.01,
    events: list[tuple] | None = None,
) -> MagicMock:
    """Make BlazeTTSClient(...) return a mock that streams `chunks` chunks.

    When `events` is given, those (data, event) pairs are yielded instead.
    """
    instance = mock_client_cls.return_value
    instance.start = AsyncMock()
    instance.stop = AsyncMock()
    instance.cancel = AsyncMock()
    instance.reset_ttfb = MagicMock()

    async def mock_get(text: str):
        if texts_seen is not None:
            texts_seen.append(text)
        if not text.strip():
            yield None, EVENT_TTS_END
            return
        if events is not None:
            for item in events:
                yield item
            return
        await asyncio.sleep(delay)
        yield int(delay * 1000), EVENT_TTS_TTFB_METRIC
        for _ in range(chunks):
            yield AUDIO_CHUNK, EVENT_TTS_RESPONSE
            await asyncio.sleep(0.001)
        yield None, EVENT_TTS_END

    instance.get.side_effect = mock_get
    return instance


def send_text(
    ten_env_tester: TenEnvTester,
    request_id: str,
    text: str,
    text_input_end: bool,
) -> None:
    tts_input = TTSTextInput(
        request_id=request_id, text=text, text_input_end=text_input_end
    )
    data = Data.create("tts_text_input")
    data.set_property_from_json(None, tts_input.model_dump_json())
    ten_env_tester.send_data(data)


class RecordingTester(ExtensionTester):
    """Sends (request_id, text, text_input_end) inputs and records output.

    Stops after `expected_ends` tts_audio_end events or on an error when
    `stop_on_error` is set.
    """

    def __init__(
        self,
        inputs: list[tuple[str, str, bool]],
        expected_ends: int = 1,
        stop_on_error: bool = False,
    ):
        super().__init__()
        self.inputs = inputs
        self.expected_ends = expected_ends
        self.stop_on_error = stop_on_error
        self.events: list[tuple[str, dict]] = []
        self.audio_bytes = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        for request_id, text, text_input_end in self.inputs:
            send_text(ten_env_tester, request_id, text, text_input_end)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        json_str, _ = data.get_property_to_json(None)
        payload = json.loads(json_str) if json_str else {}
        self.events.append((name, payload))
        if name == "error" and self.stop_on_error:
            ten_env.stop_test()
        if self.count("tts_audio_end") >= self.expected_ends:
            ten_env.stop_test()

    def on_audio_frame(self, ten_env: TenEnvTester, audio_frame):
        buf = audio_frame.lock_buf()
        try:
            self.audio_bytes += len(bytes(buf))
        finally:
            audio_frame.unlock_buf(buf)

    def count(self, name: str) -> int:
        return sum(1 for event, _ in self.events if event == name)

    def payloads(self, name: str) -> list[dict]:
        return [payload for event, payload in self.events if event == name]


# ---------------------------------------------------------------------------
# Client-level helper: an in-process Blaze realtime TTS WebSocket.
# ---------------------------------------------------------------------------


class MockBlazeTTSServer:
    """Speaks the Blaze realtime TTS protocol.

    `respond(query, ws, server)` handles each query; the default streams
    `server.audio` in chunks followed by finished-byte-stream.
    `greeting_delays` maps a 1-based connection number to seconds to wait
    before the greeting, to hold a client in its handshake.
    """

    def __init__(
        self,
        valid_token: str = "test-key",
        audio: bytes = b"\x01\x02" * 2400,
        chunk_size: int = 960,
        respond: Callable[..., Any] | None = None,
        greeting_delays: dict[int, float] | None = None,
    ):
        self.valid_token = valid_token
        self.audio = audio
        self.chunk_size = chunk_size
        self.respond = respond
        self.greeting_delays = greeting_delays or {}
        self.queries: list[dict] = []
        self.connections = 0
        self.sockets: list[Any] = []
        self._server = None
        self.port = 0

    @property
    def base_url(self) -> str:
        return f"http://127.0.0.1:{self.port}"

    async def __aenter__(self) -> "MockBlazeTTSServer":
        self._server = await websockets.serve(self._handler, "127.0.0.1", 0)
        self.port = self._server.sockets[0].getsockname()[1]
        return self

    async def __aexit__(self, *_exc) -> None:
        self._server.close()
        await self._server.wait_closed()

    @staticmethod
    async def send(ws, payload: dict) -> None:
        await ws.send(json.dumps(payload))

    async def stream(self, ws, audio: bytes | None = None) -> None:
        audio = self.audio if audio is None else audio
        await self.send(ws, {"type": "processing-request"})
        await self.send(ws, {"status": "started-byte-stream"})
        for i in range(0, len(audio), self.chunk_size):
            await ws.send(audio[i : i + self.chunk_size])
        await self.send(ws, {"type": "finished-byte-stream"})

    async def _handler(self, ws) -> None:
        self.connections += 1
        self.sockets.append(ws)
        delay = self.greeting_delays.get(self.connections)
        if delay:
            await asyncio.sleep(delay)
        try:
            await self.send(ws, {"type": "successful-connection"})
        except websockets.exceptions.ConnectionClosed:
            return
        auth = json.loads(await ws.recv())
        if auth.get("token") != self.valid_token:
            await self.send(ws, {"type": "failed-authentication"})
            await ws.close(1008, "failed-authentication")
            return
        await self.send(ws, {"type": "successful-authentication"})
        try:
            async for raw in ws:
                query = json.loads(raw)
                self.queries.append(query)
                if self.respond is not None:
                    await self.respond(query, ws, self)
                else:
                    await self.stream(ws)
        except websockets.exceptions.ConnectionClosed:
            pass


class ThreadedMockBlazeTTSServer:
    """Runs MockBlazeTTSServer on its own loop, for extension-level tests
    where the TEN runtime owns the extension's event loop."""

    def __init__(self, **options):
        import threading

        self.server = MockBlazeTTSServer(**options)
        self._loop = asyncio.new_event_loop()
        self._thread = threading.Thread(
            target=self._loop.run_forever, daemon=True
        )

    def __enter__(self) -> MockBlazeTTSServer:
        self._thread.start()
        asyncio.run_coroutine_threadsafe(
            self.server.__aenter__(), self._loop
        ).result(5)
        return self.server

    def __exit__(self, *_exc) -> None:
        asyncio.run_coroutine_threadsafe(
            self.server.__aexit__(), self._loop
        ).result(5)
        self._loop.call_soon_threadsafe(self._loop.stop)
        self._thread.join(5)
