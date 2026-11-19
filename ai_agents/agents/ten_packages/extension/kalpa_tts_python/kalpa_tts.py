#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Client for the Kalpa streaming text-to-speech WebSocket.

Protocol: `WSS /v1/tts/{voice_id}/stream`,
https://docs.kalpalabs.ai/reference#wss-v1-tts-voice-id-stream

One connection speaks in one voice. The API key rides the first message,
`initializeConnection`, which the server answers with `sessionCreated`. Text
arrives in `sendText` messages; one with `flush: true` ends the utterance.
Audio comes back as base64 16-bit PCM in `responseAudio`, and `responseDone`
ends the utterance's audio.
"""
import asyncio
import base64
import binascii
import json
import time
from typing import Any

import websockets
from websockets.exceptions import ConnectionClosed

from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_runtime import AsyncTenEnv

from .config import KalpaTTSConfig

# Deadline for the socket to open, and for `sessionCreated` after that.
SESSION_TIMEOUT_S = 10.0
# Largest message accepted. Kalpa's audio messages are tens of kilobytes.
MAX_MESSAGE_BYTES = 16 * 1024 * 1024
# Kalpa closes a connection that sends nothing for five minutes.
KEEPALIVE_INTERVAL_S = 60.0
# Consecutive failed connections before the client gives up.
MAX_CONNECT_ATTEMPTS = 5
RECONNECT_BASE_DELAY_S = 0.5
RECONNECT_MAX_DELAY_S = 8.0
# A session that stayed open this long, or delivered a message, worked. One
# that ends sooner without either counts as another failed connection.
STABLE_SESSION_S = 10.0
# Kalpa refuses to start an utterance while it is at capacity. Nothing was
# generated, so the request is sent again after at least this long.
OVERLOADED = "overloaded"
OVERLOADED_RETRY_DELAY_S = 1.0

# Failures no reconnect can fix: the key, the account, the voice or the model
# has to change first. Each has its own close code.
PERMANENT_CLOSE_CODES = {
    4401: "authentication_error",
    4402: "allowance_exhausted",
    4403: "account_suspended",
    4404: "not_found",
}
# Session settings Kalpa refuses (sample rate, model, generation parameters)
# are refused again on every retry.
INVALID_REQUEST = "invalid_request"
CLOSE_INVALID_REQUEST = 4400

_KEEPALIVE_MESSAGE = json.dumps({"type": "sendText", "text": ""})


class KalpaTTSError(Exception):
    """A failure reported by Kalpa or by the connection to it."""

    def __init__(
        self,
        error_type: str,
        message: str,
        *,
        permanent: bool = False,
        response_id: str | None = None,
        code: int = 0,
    ) -> None:
        super().__init__(message)
        self.error_type = error_type
        self.message = message
        # True when retrying cannot succeed without a configuration change.
        self.permanent = permanent
        # Set when Kalpa attributes the failure to one utterance.
        self.response_id = response_id
        # WebSocket close code, when the connection closed with one.
        self.code = code


def _error_from_message(
    message: dict[str, Any], *, opening: bool = False
) -> KalpaTTSError:
    body = message.get("error")
    if not isinstance(body, dict):
        body = {}
    error_type = str(body.get("type") or "error")
    return KalpaTTSError(
        error_type,
        str(body.get("message") or error_type),
        permanent=error_type in PERMANENT_CLOSE_CODES.values()
        or (opening and error_type == INVALID_REQUEST),
        response_id=message.get("response_id"),
    )


def _error_from_close(
    exc: ConnectionClosed, *, opening: bool = False
) -> KalpaTTSError:
    frame = exc.rcvd or exc.sent
    code = frame.code if frame else 0
    error_type = PERMANENT_CLOSE_CODES.get(code)
    if error_type is None and opening and code == CLOSE_INVALID_REQUEST:
        error_type = INVALID_REQUEST
    return KalpaTTSError(
        error_type or "connection_closed",
        (frame.reason if frame else "")
        or f"Kalpa closed the connection (code {code})",
        permanent=error_type is not None,
        code=code,
    )


class KalpaTTSClient:
    """Keeps one Kalpa session open and reports what arrives on it.

    The listener (the extension) implements `on_connecting`, `on_connected`,
    `on_disconnected`, `on_session_ready`, `on_audio`, `on_response_done`,
    `on_response_error` and `on_connection_failed`. Events are delivered one
    at a time, in the order they arrive. After `stop()` nothing more is
    delivered, so a stopped client cannot touch a later request.
    """

    def __init__(
        self, config: KalpaTTSConfig, ten_env: AsyncTenEnv, listener: Any
    ) -> None:
        self._config = config
        self._ten_env = ten_env
        self._listener: Any | None = listener
        self._ws: Any | None = None
        self._task: asyncio.Task | None = None
        self._stopped = False
        # Set while the client is ready, has given up, or has been stopped:
        # every state in which waiting longer changes nothing.
        self._settled = asyncio.Event()
        self.failed: KalpaTTSError | None = None
        # Whether the current connection has delivered a message.
        self._heard = False

    def start(self) -> None:
        if self._task is None:
            self._task = asyncio.create_task(self._run())

    def stop(self) -> None:
        """Drop the connection now. Nothing is awaited and nothing more is
        delivered, so the caller can replace the client in one step."""
        self._stopped = True
        self._listener = None
        self._settled.set()
        ws, self._ws = self._ws, None
        if self._task is not None and self._task is not asyncio.current_task():
            self._task.cancel()
        if ws is not None:
            asyncio.create_task(self._close(ws))

    async def wait_ready(self) -> bool:
        """Wait for a session. False when the client gave up or was stopped."""
        await self._settled.wait()
        return self._ws is not None

    async def send_text(self, text: str, flush: bool = False) -> None:
        """Send text, or end the utterance."""
        ws = self._ws
        if ws is None:
            raise KalpaTTSError(
                "not_connected", "The Kalpa connection is not ready"
            )
        payload: dict[str, Any] = {"type": "sendText", "text": text}
        if flush:
            payload["flush"] = True
        try:
            await ws.send(json.dumps(payload))
        except ConnectionClosed as exc:
            raise _error_from_close(exc) from exc

    async def _run(self) -> None:
        failures = 0
        loop = asyncio.get_running_loop()
        while not self._stopped:
            ws = None
            opened_at = 0.0
            self._heard = False
            try:
                await self._emit("on_connecting")
                ws = await self._open_session()
                opened_at = loop.time()
                self._ws = ws
                self._settled.set()
                await self._emit("on_connected")
                await self._emit("on_session_ready")
                error = await self._receive(ws)
            except KalpaTTSError as exc:
                error = exc
            except Exception as exc:
                # Whatever failed, the client goes on: a task that ended
                # here would leave every request waiting for a session.
                # The library may quote the address, which can hold a key.
                error = KalpaTTSError(
                    "connection_error",
                    (str(exc) or type(exc).__name__).replace(
                        self._config.websocket_url, self._config.safe_url
                    ),
                )
            finally:
                self._ws = None
                if not self._stopped:
                    # Not ready, and not given up on either: senders wait.
                    self._settled.clear()
                if ws is not None:
                    asyncio.create_task(self._close(ws))
            if self._stopped:
                return

            if ws is not None and (
                self._heard or loop.time() - opened_at >= STABLE_SESSION_S
            ):
                # The session worked, so the failures in a row start again.
                failures = 0
            failures += 1
            will_retry = not error.permanent and failures < MAX_CONNECT_ATTEMPTS
            self._ten_env.log_warn(
                f"vendor_status: connection ended ({error.error_type}): "
                f"{error.message}; retry={will_retry}",
                category=LOG_CATEGORY_VENDOR,
            )
            if not will_retry:
                self.failed = error
                self._settled.set()
            await self._emit(
                "on_disconnected", code=error.code, message=error.message
            )
            await self._emit("on_connection_failed", error, will_retry)
            if not will_retry:
                return
            delay = RECONNECT_BASE_DELAY_S * 2 ** (failures - 1)
            if error.error_type == OVERLOADED:
                delay = max(delay, OVERLOADED_RETRY_DELAY_S)
            await asyncio.sleep(min(delay, RECONNECT_MAX_DELAY_S))

    async def _open_session(self) -> Any:
        ws = await websockets.connect(
            self._config.websocket_url,
            open_timeout=SESSION_TIMEOUT_S,
            max_size=MAX_MESSAGE_BYTES,
        )
        try:
            # Never log this message: it carries the API key.
            await ws.send(json.dumps(self._config.session_message()))
            raw = await asyncio.wait_for(ws.recv(), SESSION_TIMEOUT_S)
            message = json.loads(raw)
            if not isinstance(message, dict):
                raise ValueError("not a JSON object")
            if message.get("type") == "error":
                raise _error_from_message(message, opening=True)
            if message.get("type") != "sessionCreated":
                raise ValueError("no sessionCreated")
        except ConnectionClosed as exc:
            raise _error_from_close(exc, opening=True) from exc
        except ValueError as exc:
            asyncio.create_task(self._close(ws))
            raise KalpaTTSError(
                "protocol_error", f"Unreadable session reply: {exc}"
            ) from exc
        except BaseException:
            asyncio.create_task(self._close(ws))
            raise
        self._ten_env.log_debug(
            f"vendor_status: session created "
            f"(session_id={message.get('session_id')}, "
            f"sample_rate={message.get('sample_rate')})",
            category=LOG_CATEGORY_VENDOR,
        )
        return ws

    async def _receive(self, ws: Any) -> KalpaTTSError:
        """Deliver messages until the connection ends; return why it ended."""
        keepalive = asyncio.create_task(self._keepalive(ws))
        try:
            while True:
                raw = await ws.recv()
                # Taken before any decoding: it ends the TTFB interval.
                received_at = time.monotonic()
                try:
                    message = json.loads(raw)
                except (TypeError, ValueError):
                    message = None
                if not isinstance(message, dict):
                    continue
                self._heard = True
                kind = message.get("type")
                if kind == "responseAudio":
                    pcm = message.get("pcm_b64")
                    try:
                        pcm = base64.b64decode(pcm or "", validate=True)
                    except (binascii.Error, TypeError, ValueError):
                        # Audio that cannot be read is audio lost.
                        await self._emit(
                            "on_response_error",
                            KalpaTTSError(
                                "protocol_error", "Unreadable audio from Kalpa"
                            ),
                        )
                        continue
                    if pcm:
                        await self._emit("on_audio", pcm, received_at)
                elif kind == "responseDone":
                    await self._emit(
                        "on_response_done",
                        str(message.get("status") or "completed"),
                    )
                elif kind == "error":
                    error = _error_from_message(message)
                    # Kalpa closes the connection after a fatal error. After
                    # an `overloaded` refusal it may keep it and cancel the
                    # response; a new connection takes the request again
                    # either way.
                    if message.get("fatal") or error.error_type == OVERLOADED:
                        return error
                    await self._emit("on_response_error", error)
        except ConnectionClosed as exc:
            return _error_from_close(exc)
        finally:
            keepalive.cancel()

    async def _keepalive(self, ws: Any) -> None:
        """An empty `sendText` is a no-op for Kalpa and resets its idle timer."""
        try:
            while True:
                await asyncio.sleep(KEEPALIVE_INTERVAL_S)
                await ws.send(_KEEPALIVE_MESSAGE)
        except (ConnectionClosed, OSError):
            return

    async def _emit(self, name: str, *args: Any, **kwargs: Any) -> None:
        listener = self._listener
        if listener is None:
            return
        try:
            await getattr(listener, name)(*args, **kwargs)
        except Exception as exc:
            # A failing handler must not take the connection down with it.
            self._ten_env.log_error(
                f"Kalpa TTS listener {name} failed: {type(exc).__name__}: {exc}"
            )

    @staticmethod
    async def _close(ws: Any) -> None:
        try:
            await ws.close()
        except Exception:
            pass
