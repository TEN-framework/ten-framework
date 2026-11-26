#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""A local Kalpa TTS WebSocket server with scripted failures.

It speaks the protocol of `WSS /v1/tts/{voice_id}/stream`, so the tests run
the extension's real client against it. It runs on its own thread and loop.
"""
import asyncio
import base64
import json
import re
import threading
import time
from typing import Any

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

API_KEY = "test-key"
SAMPLE_RATE = 24000
# Three chunks. The first two split a 16-bit sample between them.
CHUNKS = [
    b"\x01\x00" * 480 + b"\x02",
    b"\x00" + b"\x03\x00" * 480,
    b"\x04\x00" * 480,
]
AUDIO = b"".join(CHUNKS)
AUDIO_MS = len(AUDIO) * 1000 // (2 * SAMPLE_RATE)
# What is heard of a response that stops after its first chunk: the byte
# that begins a sample is held back.
FIRST = CHUNKS[0][:-1]

# A sentence Kalpa takes as complete: its closing mark, then whitespace.
SENTENCE_END = re.compile(r"[.!?।]+[)\]\"'”’»]*\s")
# With `generation_config`, an utterance starts speaking once its text is
# this long and holds a complete sentence, or at its flush.
SEGMENT_THRESHOLD = 50


class MockKalpaServer:
    def __init__(self) -> None:
        # One entry per connection: when it opened, its `initializeConnection`
        # message, the utterances it flushed and its keepalives.
        self.sessions: list[dict[str, Any]] = []

        # Scripted behaviour, set by a test:
        # the next session is not created until `release()`;
        self.hold_session = False
        # the next N connections are closed before their session is created;
        self.fail_sessions = 0
        # every session is refused with this error type and close code;
        self.refuse_sessions: tuple[str, int] | None = None
        # the next N connections drop when their first text arrives;
        self.drop_on_text = 0
        # how the next responses go, one entry each, instead of completing:
        # "error" fails after the flush, "error_mid" after one chunk and
        # "error_early" at a start before the flush, "cancel" ends cancelled
        # with no error, "overload" is refused at its start, "drop_mid"
        # loses the connection after one chunk, "corrupt" sends audio that
        # is not base64, "noise" sends frames that are not Kalpa messages;
        self.responses: list[str] = []
        # the audio of every response, and the pause between its chunks;
        self.chunks = CHUNKS
        self.chunk_delay_s = 0.005
        # the next response waits, once it has sent each of these numbers
        # of chunks, until `release()` or until its connection closes.
        self.holds: list[int] = []

        self._count = 0
        self._loop: asyncio.AbstractEventLoop | None = None
        self._stop: asyncio.Event | None = None
        self._released: asyncio.Event | None = None
        self._port = 0
        self._ready = threading.Event()
        self._thread = threading.Thread(target=self._serve, daemon=True)

    @property
    def url(self) -> str:
        return f"ws://127.0.0.1:{self._port}"

    @property
    def utterances(self) -> list[str]:
        return [text for s in self.sessions for text in s["utterances"]]

    def start(self) -> "MockKalpaServer":
        self._thread.start()
        self._ready.wait(5)
        return self

    def stop(self) -> None:
        self._loop.call_soon_threadsafe(self._stop.set)
        self._thread.join(5)

    def release(self) -> None:
        """Let a session or response that is being held go on."""
        self._loop.call_soon_threadsafe(self._released.set)

    def _serve(self) -> None:
        asyncio.run(self._main())

    async def _main(self) -> None:
        self._loop = asyncio.get_running_loop()
        self._stop = asyncio.Event()
        self._released = asyncio.Event()
        async with serve(self._handle, "127.0.0.1", 0) as server:
            self._port = server.sockets[0].getsockname()[1]
            self._ready.set()
            await self._stop.wait()

    async def _handle(self, ws: Any) -> None:
        session: dict[str, Any] = {"opened_at": time.monotonic(), "init": None}
        session.update(utterances=[], keepalives=0)
        self.sessions.append(session)
        try:
            init = session["init"] = json.loads(await ws.recv())
            hold, self.hold_session = self.hold_session, False
            if hold and not await self._wait_for_release(ws):
                return
            if self.fail_sessions > 0:
                self.fail_sessions -= 1
                await ws.close(code=1011, reason="internal error")
                return
            refusal = self.refuse_sessions
            if refusal is None and init.get("api_key") != API_KEY:
                refusal = ("authentication_error", 4401)
            if refusal is not None:
                await self._error(ws, refusal[0], fatal=True)
                await ws.close(code=refusal[1], reason=refusal[0])
                return
            await self._send(ws, type="sessionCreated", session_id="session")
            await self._converse(ws, session)
        except ConnectionClosed:
            pass

    async def _converse(self, ws: Any, session: dict[str, Any]) -> None:
        segmented = session["init"].get("generation_config") is not None
        buffer = ""
        # The id of the response that has started, and how it goes.
        started: tuple[str, str] | None = None
        async for raw in ws:
            message = json.loads(raw)
            if message.get("type") != "sendText":
                continue
            text = message.get("text") or ""
            flush = bool(message.get("flush"))
            if text.strip() and self.drop_on_text > 0:
                self.drop_on_text -= 1
                ws.transport.abort()
                return
            buffer += text
            session["keepalives"] += not text and not flush
            # Chunks sent before the flush: the complete sentences are
            # spoken as soon as there are enough of them.
            spoken = int(started is not None)
            if started is None and (
                flush
                or segmented
                and len(buffer.strip()) >= SEGMENT_THRESHOLD
                and SENTENCE_END.search(buffer)
            ):
                self._count += 1
                started = (
                    f"resp_{self._count}",
                    self.responses.pop(0) if self.responses else "",
                )
                if started[1] in ("overload", "error_early"):
                    # Before the flush Kalpa cannot tell this utterance's
                    # text from the next one's, so a failure ends the
                    # connection. After the flush the connection stays open.
                    kind = {"overload": "overloaded"}.get(
                        started[1], "inference_error"
                    )
                    await self._error(ws, kind, not flush, started[0])
                    if not flush:
                        await ws.close(code=1011, reason="utterance failed")
                        return
                await self._send(
                    ws, type="responseCreated", response_id=started[0]
                )
                if not flush:
                    await self._audio(ws, started[0], 0)
            if flush:
                (response_id, how), started = started, None
                session["utterances"].append(buffer)
                buffer = ""
                if not await self._respond(ws, response_id, how, spoken):
                    return

    async def _respond(
        self, ws: Any, response_id: str, how: str, spoken: int
    ) -> bool:
        """Send the chunks after the first `spoken`, then the end. False
        when the connection is gone."""
        if how in ("error", "cancel", "overload"):
            if how == "error":
                await self._error(ws, "inference_error", False, response_id)
            await self._done(ws, response_id, "cancelled")
            return True
        if how == "noise":
            await ws.send("not JSON")
            await ws.send("[1, 2, 3]")
            await ws.send(b"\x00\x01\x02")
            await self._send(ws, type="somethingNew", response_id=response_id)
        if how == "corrupt":
            await self._send(ws, type="responseAudio", pcm_b64="not base64!")
        holds, self.holds = self.holds, []
        for index in range(spoken, len(self.chunks) + 1):
            if index in holds and not await self._wait_for_release(ws):
                return False
            if index == 1 and how == "error_mid":
                await self._error(ws, "inference_error", False, response_id)
                await self._done(ws, response_id, "cancelled")
                return True
            if index == 1 and how == "drop_mid":
                await asyncio.sleep(0.05)
                ws.transport.abort()
                return False
            if index < len(self.chunks):
                await self._audio(ws, response_id, index)
                await asyncio.sleep(self.chunk_delay_s)
        await self._done(ws, response_id, "completed")
        return True

    async def _wait_for_release(self, ws: Any) -> bool:
        """False when the connection closed before `release()`."""
        released = asyncio.ensure_future(self._released.wait())
        closed = asyncio.ensure_future(ws.wait_closed())
        await asyncio.wait(
            {released, closed}, return_when=asyncio.FIRST_COMPLETED
        )
        for waiter in (released, closed):
            waiter.cancel()
        if not self._released.is_set():
            return False
        self._released.clear()
        return True

    async def _audio(self, ws: Any, response_id: str, index: int) -> None:
        await self._send(
            ws,
            type="responseAudio",
            response_id=response_id,
            pcm_b64=base64.b64encode(self.chunks[index]).decode(),
        )

    async def _error(
        self, ws: Any, error_type: str, fatal: bool, response_id: str = ""
    ) -> None:
        message: dict[str, Any] = {
            "type": "error",
            "fatal": fatal,
            "error": {"type": error_type, "message": f"Kalpa: {error_type}."},
        }
        if response_id:
            message["response_id"] = response_id
        await ws.send(json.dumps(message))

    async def _done(self, ws: Any, response_id: str, status: str) -> None:
        await self._send(
            ws, type="responseDone", response_id=response_id, status=status
        )

    @staticmethod
    async def _send(ws: Any, **message: Any) -> None:
        await ws.send(json.dumps(message))
