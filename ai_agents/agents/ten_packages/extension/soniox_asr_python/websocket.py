import asyncio
import json
import random
import time
from dataclasses import dataclass
from enum import Enum
from typing import Callable, Optional

import websockets


@dataclass
class SonioxTranscriptToken:
    text: str
    start_ms: int
    end_ms: int
    is_final: Optional[bool] = None
    confidence: Optional[float] = None
    speaker: Optional[str] = None
    translation_status: Optional[str] = None
    language: Optional[str] = None


@dataclass
class SonioxTranslationToken:
    text: str
    translation_status: str
    language: str
    source_language: str
    is_final: Optional[bool] = None
    confidence: Optional[float] = None
    speaker: Optional[str] = None


@dataclass
class SonioxFinToken:
    text: str
    is_final: bool


class SonioxWebsocketEvents(Enum):
    EXCEPTION = "exception"
    OPEN = "open"
    CLOSE = "close"
    ERROR = "error"
    FINISHED = "finished"
    TRANSCRIPT = "transcript"


class SonioxWebsocketClient:
    class State(Enum):
        INIT = "init"
        CONNECTING = "connecting"
        CONNECTED = "connected"
        STOPPING = "stopping"
        STOPPED = "stopped"

    def __init__(
        self,
        url: str,
        start_request: str,
        base_delay: float = 0.1,
        max_delay: float = 10.0,
        max_attempts: int = 10,
        enable_keepalive: bool = False,
        keepalive_interval: float = 15.0,
    ):
        self.url = url
        self.start_request = start_request
        self.state = self.State.INIT
        self._send_queue = asyncio.Queue()
        self._stop_event = asyncio.Event()
        self._event_callbacks = {}

        # Exponential backoff parameters
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.max_attempts = max_attempts
        self._attempt_count = 0

        # Keepalive parameters
        self.enable_keepalive = enable_keepalive
        self.keepalive_interval = keepalive_interval
        self._last_audio_time = 0.0
        self._keepalive_task = None

    async def connect(self):
        self._reset_client_state()
        while (
            self.state != self.State.STOPPED
            and self.state != self.State.STOPPING
        ):
            try:
                self._reset_session_state()
                self.state = self.State.CONNECTING
                async with websockets.connect(self.url) as ws:
                    await ws.send(self.start_request)
                    self.state = self.State.CONNECTED
                    await self._call(SonioxWebsocketEvents.OPEN)
                    # Start keepalive task when connection is established and keepalive is enabled
                    if self.enable_keepalive:
                        self._start_keepalive_task(ws)
                    await self._work(ws)
            except Exception as e:
                await self._call(SonioxWebsocketEvents.EXCEPTION, e)
                await self._call(SonioxWebsocketEvents.CLOSE)
                await self._exponential_backoff()
            else:
                await self._call(SonioxWebsocketEvents.CLOSE)

        if self.state == self.State.STOPPING:
            self._stop_event.set()

    def _reset_client_state(self):
        self.state = self.State.INIT
        self._attempt_count = 0

    def _reset_session_state(self):
        self._stop_event.clear()
        if self.enable_keepalive:
            self._last_audio_time = 0.0
            self._stop_keepalive_task()

    def _start_keepalive_task(self, ws):
        """Start the keepalive background task"""
        if self._keepalive_task is not None:
            self._keepalive_task.cancel()
        self._keepalive_task = asyncio.create_task(self._keepalive_loop(ws))

    def _stop_keepalive_task(self):
        """Stop the keepalive background task"""
        if self._keepalive_task is not None:
            self._keepalive_task.cancel()
            self._keepalive_task = None

    # pylint: disable=unused-argument
    async def _keepalive_loop(self, ws):
        """Background task that sends keepalive messages when no audio data for keepalive_interval seconds"""
        try:
            while self.state == self.State.CONNECTED:
                current_time = time.time()
                time_since_last_audio = current_time - self._last_audio_time

                if time_since_last_audio >= self.keepalive_interval:
                    # Send keepalive message
                    keepalive_message = json.dumps({"type": "keepalive"})
                    await self._send_queue.put(keepalive_message)
                    # Reset the timer after sending keepalive
                    self._last_audio_time = current_time

                # Wait for a short interval before checking again
                await asyncio.sleep(1.0)
        except asyncio.CancelledError:
            # Task was cancelled, exit gracefully
            pass
        except Exception as e:
            # Log any unexpected errors in keepalive loop
            await self._call(SonioxWebsocketEvents.EXCEPTION, e)

    async def _exponential_backoff(self):
        if self._attempt_count >= self.max_attempts:
            self.state = self.State.STOPPED
            await self._call(
                SonioxWebsocketEvents.EXCEPTION,
                Exception("max attempts reached"),
            )
            return

        self._attempt_count += 1

        delay = min(
            self.base_delay * (2 ** (self._attempt_count - 1)), self.max_delay
        )

        jitter = random.uniform(0, 0.1 * delay)
        final_delay = delay + jitter

        await asyncio.sleep(final_delay)

    async def _work(self, ws):
        while self.state != self.State.STOPPED:
            recv_task = asyncio.create_task(ws.recv())
            send_task = asyncio.create_task(self._send_queue.get())
            done, pending = await asyncio.wait(
                [recv_task, send_task], return_when=asyncio.FIRST_COMPLETED
            )
            for task in pending:
                task.cancel()
            for task in done:
                if task is recv_task:
                    await self._handle_recv(ws, task.result())
                elif task is send_task:
                    await self._handle_send(ws, task.result())

    # pylint: disable=unused-argument
    async def _handle_recv(self, ws, message: str):
        data = json.loads(message)
        match data:
            case {"error_code": error_code, "error_message": error_message}:
                await self._call(
                    SonioxWebsocketEvents.ERROR,
                    error_code,
                    error_message,
                )
            case {
                "finished": True,
                "final_audio_proc_ms": final_audio_proc_ms,
                "total_audio_proc_ms": total_audio_proc_ms,
            }:
                await self._call(
                    SonioxWebsocketEvents.FINISHED,
                    final_audio_proc_ms,
                    total_audio_proc_ms,
                )
                self._stop_event.set()
            case {
                "tokens": tokens,
                "final_audio_proc_ms": final_audio_proc_ms,
                "total_audio_proc_ms": total_audio_proc_ms,
            }:
                await self._call(
                    SonioxWebsocketEvents.TRANSCRIPT,
                    self._parse_tokens(tokens),
                    final_audio_proc_ms,
                    total_audio_proc_ms,
                )

    def _parse_tokens(
        self, tokens: list[dict]
    ) -> list[SonioxTranscriptToken | SonioxTranslationToken | SonioxFinToken]:
        return [self._parse_token(token) for token in tokens]

    def _parse_token(
        self, token: dict
    ) -> SonioxTranscriptToken | SonioxTranslationToken | SonioxFinToken:
        match token:
            case {"text": "<fin>", "is_final": True}:
                return SonioxFinToken("<fin>", True)
            case {
                "text": text,
                "translation_status": translation_status,
                "language": language,
                "source_language": source_language,
                **optionals,
            }:
                return SonioxTranslationToken(
                    text,
                    translation_status,
                    language,
                    source_language,
                    **optionals,
                )
            case {
                "text": text,
                "start_ms": start_ms,
                "end_ms": end_ms,
                "is_final": is_final,
                **optionals,
            }:
                return SonioxTranscriptToken(
                    text, start_ms, end_ms, is_final, **optionals
                )
        assert False, f"Invalid token: {token}"

    async def _handle_send(self, ws, message: str):
        await ws.send(message)

    async def finalize(self):
        await self._send_queue.put(json.dumps({"type": "finalize"}))

    async def stop(self, wait: bool = True):
        self.state = self.State.STOPPING
        if self.enable_keepalive:
            self._stop_keepalive_task()
        await self._send_queue.put("")
        if wait:
            try:
                await asyncio.wait_for(self._stop_event.wait(), timeout=5.0)
            except asyncio.TimeoutError as e:
                await self._call(SonioxWebsocketEvents.EXCEPTION, e)
            finally:
                self.state = self.State.STOPPED

    async def send_audio(self, audio: bytes):
        if self.enable_keepalive:
            self._last_audio_time = time.time()
        await self._send_queue.put(audio)

    def on(self, event: SonioxWebsocketEvents, callback: Callable):
        """
        Register a callback for a specific event.
        The callback should be a coroutine function that takes the same arguments as the event.
        EXCEPTION:
            - Exception: the exception object
        OPEN:
            - None
        CLOSE:
            - None
        ERROR:
            - error_code: int
            - error_message: str
        FINISHED:
            - final_audio_proc_ms: int
            - total_audio_proc_ms: int
        TRANSCRIPT:
            - tokens: list[SonioxTranscriptToken | SonioxTranslationToken | SonioxFinToken]
            - final_audio_proc_ms: int
            - total_audio_proc_ms: int
        """
        self._event_callbacks[event] = callback

    async def _call(self, event: SonioxWebsocketEvents, *args, **kwargs):
        if event in self._event_callbacks:
            await self._event_callbacks[event](*args, **kwargs)
