"""One Prism WebSocket session with bounded startup and greeting readiness."""

import asyncio
import base64
import json
import uuid
from collections import deque

import aiohttp


class PrismError(Exception):
    """A sanitized provider failure and whether the session must end."""

    def __init__(self, code: str, message: str, fatal: bool = False):
        super().__init__(message)
        self.code = code
        self.fatal = fatal


class PrismConnection:
    """Own transport tasks; established sessions never reconnect or replay."""

    def __init__(self, config, on_event):
        self.config = config
        self.on_event = on_event
        self.ready = False
        self.stopping = False
        self._session = None
        self._socket = None
        self._receiver = None
        self._initialized = None
        self._setup_id = None
        self._write_lock = asyncio.Lock()
        self._typed_ready = False
        self._triggers: deque[dict] = deque()
        self._trigger_inflight = None
        self._generation = 0
        self._response_id = None
        self._interrupted = False

    @staticmethod
    def event_id():
        """Generate a request identity for correlation, never implicit replay."""
        return "evt_" + uuid.uuid4().hex

    async def start(self):
        """Retry only transient startup failures within one time budget."""
        if not self.config.api_key.get_secret_value():
            raise PrismError("missing_api_key", "Rime API key is required")
        loop = asyncio.get_running_loop()
        deadline = loop.time() + self.config.startup_timeout
        for attempt in range(self.config.startup_retries + 1):
            self._initialized = loop.create_future()
            try:
                self._session = aiohttp.ClientSession(
                    timeout=aiohttp.ClientTimeout(total=None, sock_connect=5)
                )
                self._socket = await asyncio.wait_for(
                    self._session.ws_connect(
                        self.config.endpoint,
                        headers={
                            "Authorization": "Bearer "
                            + self.config.api_key.get_secret_value()
                        },
                        heartbeat=15,
                        max_msg_size=1024 * 1024,
                    ),
                    max(0, deadline - loop.time()),
                )
                self._receiver = asyncio.create_task(self._receive())
                await asyncio.wait_for(
                    asyncio.shield(self._initialized), max(0, deadline - loop.time())
                )
                return
            except aiohttp.ClientResponseError as error:
                retry = error.status in {408, 429, 500, 502, 503, 504}
                failure = PrismError(str(error.status), "Prism refused the connection")
            except (aiohttp.ClientConnectionError, asyncio.TimeoutError):
                retry = True
                failure = PrismError("startup_timeout", "Prism startup failed")
            except PrismError as error:
                retry = error.code == "connection_closed"
                failure = error
            except BaseException:
                await self._close_transport()
                raise
            await self._close_transport()
            delay = min(0.25 * 2**attempt, 1)
            if (
                not retry
                or attempt == self.config.startup_retries
                or loop.time() + delay >= deadline
                or self.stopping
            ):
                raise failure
            await asyncio.sleep(delay)

    async def send(self, payload):
        """Serialize writes with a deadline, including writer-lock acquisition."""
        request = {**payload, "event_id": payload.get("event_id", self.event_id())}

        async def write():
            async with self._write_lock:
                if self._socket is None or self._socket.closed or self.stopping:
                    raise PrismError("connection_closed", "Prism is disconnected", True)
                await self._socket.send_json(request)

        try:
            await asyncio.wait_for(write(), self.config.startup_timeout)
        except (aiohttp.ClientError, OSError, asyncio.TimeoutError) as error:
            raise PrismError(
                "connection_closed", "Prism write failed; start a new call", True
            ) from error
        return request["event_id"]

    async def send_audio(self, audio):
        """Send headerless 16 kHz mono little-endian PCM16, including silence."""
        if not self.ready:
            raise PrismError("not_ready", "Prism is not initialized")
        await self.send(
            {
                "type": "input_audio_buffer.append",
                "audio": base64.b64encode(audio).decode("ascii"),
            }
        )

    async def trigger(self, text):
        """Queue the standard TEN user-text/greeting trigger until Prism is idle."""
        if len(self._triggers) >= 8:
            raise PrismError("trigger_queue_full", "Too many pending replies")
        request = {"metadata": {"prism_cause": "proactive"}}
        if text is not None:
            request = {
                "metadata": {"prism_cause": "user_text"},
                "prism_input_text": text,
            }
        self._triggers.append(request)
        await self._send_trigger()

    async def _send_trigger(self):
        if (
            self.ready
            and self._typed_ready
            and self._triggers
            and self._trigger_inflight is None
        ):
            request = self._triggers.popleft()
            event_id = self.event_id()
            self._typed_ready = False
            self._trigger_inflight = (event_id, request, self._generation)
            await self.send(
                {"type": "response.create", "event_id": event_id, "response": request}
            )

    async def _dispatch(self, event):
        kind = event["type"]
        request_id = event.get("prism_request_event_id")
        if kind == "error":
            error = event["error"]
            request_id = error.get("event_id")
            if error.get("scope") == "session" or (
                not self.ready and request_id == self._setup_id
            ):
                raise PrismError(
                    error.get("code", "session_error"), "Prism refused the session"
                )
            if self._trigger_inflight and (
                request_id == self._trigger_inflight[0]
                or error.get("code") == "typed_turn_superseded"
            ):
                _, request, generation = self._trigger_inflight
                self._trigger_inflight = None
                if (
                    error.get("code") == "typed_turn_busy"
                    and generation == self._generation
                ):
                    self._triggers.appendleft(request)
                    self._typed_ready = False
        elif kind == "session.created":
            if self._setup_id is not None:
                raise PrismError("protocol_error", "Duplicate session.created")
            self._setup_id = self.event_id()
            await self.send(
                {
                    "type": "session.update",
                    "event_id": self._setup_id,
                    "session": self.config.session_settings(),
                }
            )
        elif kind == "session.updated" and not self.ready:
            if request_id != self._setup_id:
                raise PrismError("protocol_error", "Unmatched session update")
            self.ready = True
            self._initialized.set_result(None)
        elif kind == "prism.typed_input.ready":
            self._typed_ready = True
        elif kind == "response.created":
            self._response_id = event["response"]["id"]
            self._interrupted = False
            self._typed_ready = False
            if self._trigger_inflight and request_id == self._trigger_inflight[0]:
                self._trigger_inflight = None
        elif kind == "input_audio_buffer.speech_started":
            self._generation += 1
            self._interrupted = True
            self._triggers.clear()
            self._typed_ready = False
        elif kind in {"response.audio.delta", "response.text.done"}:
            if self._response_id is None:
                raise PrismError("protocol_error", "Output has no response identity")
            if self._interrupted or event["response_id"] != self._response_id:
                return
        await self.on_event(event)
        await self._send_trigger()

    def _redact(self, value):
        if isinstance(value, str):
            key = self.config.api_key.get_secret_value()
            return value.replace(key, "[redacted]") if key else value
        if isinstance(value, dict):
            return {key: self._redact(item) for key, item in value.items()}
        if isinstance(value, list):
            return [self._redact(item) for item in value]
        return value

    async def _receive(self):
        try:
            async for message in self._socket:
                if message.type != aiohttp.WSMsgType.TEXT:
                    raise ValueError("expected JSON")
                event = json.loads(message.data)
                if not isinstance(event, dict) or not isinstance(
                    event.get("type"), str
                ):
                    raise ValueError("event needs a string type")
                await self._dispatch(self._redact(event))
            raise PrismError("connection_closed", "Prism ended the conversation")
        except Exception as error:
            failure = (
                error
                if isinstance(error, PrismError)
                else PrismError("protocol_error", "Prism event processing failed")
            )
            if not self._initialized.done():
                self._initialized.set_exception(failure)
            elif not self.stopping:
                await self.on_event(
                    {
                        "type": "error",
                        "error": {"code": failure.code, "scope": "session"},
                    }
                )
        finally:
            self.ready = False
            if self._socket is not None:
                await self._socket.close()
            if self._session is not None:
                await self._session.close()

    async def _close_transport(self):
        if self._receiver is not None:
            if self._receiver is not asyncio.current_task():
                self._receiver.cancel()
                await asyncio.gather(self._receiver, return_exceptions=True)
            self._receiver = None
        if self._socket is not None:
            await self._socket.close()
        if self._session is not None:
            await self._session.close()
        if self._initialized is not None and not self._initialized.done():
            self._initialized.cancel()
        self._setup_id = None
        self.ready = False
        self._typed_ready = False

    async def close(self):
        """Cancel and join owned transport work without replaying requests."""
        self.stopping = True
        await self._close_transport()
