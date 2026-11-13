import asyncio
import json
import uuid
from collections.abc import Awaitable, Callable
from typing import Any
from urllib.parse import urlparse, urlunparse

import websockets
from websockets.asyncio.client import ClientConnection
from websockets.exceptions import InvalidStatus

EventHandler = Callable[[dict[str, Any]], Awaitable[None]]
DisconnectHandler = Callable[["SpekoRouterError | None"], Awaitable[None]]


class SpekoRouterError(Exception):
    def __init__(
        self,
        code: str,
        message: str,
        *,
        retryable: bool = False,
        request_id: str = "",
        hint: str = "",
    ) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.retryable = retryable
        self.request_id = request_id
        self.hint = hint

    @classmethod
    def from_event(cls, event: dict[str, Any]) -> "SpekoRouterError":
        error = event.get("error", {})
        return cls(
            str(error.get("code", "relay_error")),
            str(error.get("message", "Speko Router error")),
            retryable=bool(error.get("retryable", False)),
            request_id=str(error.get("request_id", "")),
            hint=str(error.get("hint", "")),
        )


class SpekoCommitTimeout(SpekoRouterError):
    """input.commit got no transcript.final; the session itself is intact."""


# Set on a transcript.final that answers a commit whose caller stopped waiting
# (timeout); holds the context that caller passed to commit().
LATE_COMMIT_CONTEXT = "speko_late_commit_context"


class _PendingCommit:
    """An input.commit sent and not yet answered by a transcript.final."""

    def __init__(self, commit_id: str, context: Any) -> None:
        self.answered: asyncio.Future[None] = (
            asyncio.get_running_loop().create_future()
        )
        # An abandoned commit's failure has nobody awaiting it.
        self.answered.add_done_callback(
            lambda future: future.cancelled() or future.exception()
        )
        self.commit_id = commit_id
        self.context = context
        self.abandoned = False


def websocket_url(base_url: str, path: str) -> str:
    parsed = urlparse(base_url)
    scheme = {"https": "wss", "http": "ws"}.get(parsed.scheme, parsed.scheme)
    base_path = parsed.path.rstrip("/")
    return urlunparse((scheme, parsed.netloc, f"{base_path}{path}", "", "", ""))


class SpekoASRClient:
    def __init__(
        self,
        *,
        api_key: str,
        base_url: str,
        configure: dict[str, Any],
        ready_timeout_sec: float,
        finalize_timeout_sec: float,
        on_event: EventHandler,
        on_disconnect: DisconnectHandler,
    ) -> None:
        self.api_key = api_key
        self.url = websocket_url(base_url, "/v1/stt/stream")
        self.configure = configure
        self.ready_timeout_sec = ready_timeout_sec
        self.finalize_timeout_sec = finalize_timeout_sec
        self.on_event = on_event
        self.on_disconnect = on_disconnect

        self.request_id = ""
        self.route: dict[str, Any] = {}
        self.usage: dict[str, Any] = {}
        self._ws: ClientConnection | None = None
        self._listener_task: asyncio.Task[None] | None = None
        # Sent commits by commit_id, oldest first. The Router answers each
        # input.commit with exactly one transcript.final.
        self._pending_commits: dict[str, _PendingCommit] = {}
        # True: the Router stamps each answer with its commit's commit_id, so
        # only that final answers it and a final without one is the provider's
        # own. False: an older Router answers commits in order. None: this
        # session has not shown which yet.
        self._correlated: bool | None = None
        self._audio_since_commit = False
        self._ready = False
        self._closing = False

    @property
    def is_ready(self) -> bool:
        return self._ready and not self._closing and self._ws is not None

    async def connect(self) -> None:
        if self.is_ready:
            return

        self._closing = False
        headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Idempotency-Key": uuid.uuid4().hex,
        }
        try:
            self._ws = await websockets.connect(
                self.url,
                additional_headers=headers,
                open_timeout=self.ready_timeout_sec,
                close_timeout=2.0,
            )
            configure_frame = json.dumps(self.configure, separators=(",", ":"))
            await self._ws.send(configure_frame)
            raw = await asyncio.wait_for(
                self._ws.recv(), timeout=self.ready_timeout_sec
            )
            event = self._decode_text_event(raw)
            if event.get("type") == "error":
                raise SpekoRouterError.from_event(event)
            if event.get("type") != "session.ready":
                raise SpekoRouterError(
                    "relay_error",
                    "Expected session.ready from Speko Router",
                    retryable=True,
                )
            self.request_id = str(event.get("request_id", ""))
            self.route = dict(event.get("route", {}))
            self._pending_commits.clear()
            self._correlated = True if event.get("commit_ids") is True else None
            self._audio_since_commit = False
            self._ready = True
            await self.on_event(event)
            self._listener_task = asyncio.create_task(self._listen())
        except InvalidStatus as error:
            await self._close_socket()
            try:
                event = json.loads(error.response.body)
            except (ValueError, TypeError):
                event = {}
            if isinstance(event, dict) and isinstance(event.get("error"), dict):
                raise SpekoRouterError.from_event(event) from error
            status = error.response.status_code
            code = {
                400: "invalid_request",
                401: "authentication_failed",
                403: "authentication_failed",
                402: "insufficient_credit",
                404: "route_not_found",
                429: "rate_limited",
            }.get(status, "relay_error")
            raise SpekoRouterError(
                code,
                f"Speko Router WebSocket upgrade failed ({status})",
                retryable=status >= 500 or status == 429,
            ) from error
        except (Exception, asyncio.CancelledError):
            await self._close_socket()
            raise

    async def send_audio(self, audio: bytes) -> None:
        if not self.is_ready or self._ws is None:
            raise SpekoRouterError(
                "relay_error", "Speko ASR session is not ready", retryable=True
            )
        await self._ws.send(audio)
        if audio:
            self._audio_since_commit = True

    async def commit(self, context: Any = None) -> None:
        """Finalize the audio sent since the last commit.

        Returns when the Router answers this commit. A provider final that
        arrives first does not show where its audio ends, so any audio sent
        since the last commit is committed and its own answer awaited.

        Raises SpekoCommitTimeout when no answer came but the session is still
        usable. Without commit ids a missing answer would shift every later
        pairing, so the timeout is a plain SpekoRouterError: reset the session.
        """
        if not self.is_ready or self._ws is None:
            raise SpekoRouterError(
                "relay_error", "Speko ASR session is not ready", retryable=True
            )
        if not self._audio_since_commit:
            return
        pending = _PendingCommit(uuid.uuid4().hex, context)
        self._pending_commits[pending.commit_id] = pending
        try:
            await self._ws.send(
                json.dumps(
                    {"type": "input.commit", "commit_id": pending.commit_id},
                    separators=(",", ":"),
                )
            )
        except BaseException:
            # Never sent, so the Router owes it no answer.
            del self._pending_commits[pending.commit_id]
            raise
        self._audio_since_commit = False
        try:
            await asyncio.wait_for(
                asyncio.shield(pending.answered),
                timeout=self.finalize_timeout_sec,
            )
        except asyncio.TimeoutError as error:
            # Stays outstanding: its late final must answer it, not the next.
            pending.abandoned = True
            if self._correlated:
                raise SpekoCommitTimeout(
                    "request_timeout",
                    "Timed out waiting for a final Speko transcript",
                    retryable=True,
                ) from error
            raise SpekoRouterError(
                "request_timeout",
                "Timed out waiting for a final Speko transcript; in-order "
                "answers can no longer be paired",
                retryable=True,
            ) from error
        except asyncio.CancelledError:
            pending.abandoned = True
            raise

    async def close(self, *, drain: bool = True) -> None:
        self._closing = True
        if self._ws is not None and self._ready:
            try:
                await self._ws.send('{"type":"session.close"}')
            except Exception:
                pass
        task = self._listener_task
        try:
            if (
                drain
                and task is not None
                and task is not asyncio.current_task()
            ):
                await asyncio.wait_for(asyncio.shield(task), timeout=2.0)
        except asyncio.TimeoutError:
            pass
        finally:
            await self._close_socket()

    async def _listen(self) -> None:
        error: SpekoRouterError | None = None
        try:
            assert self._ws is not None
            async for raw in self._ws:
                if isinstance(raw, bytes):
                    raise SpekoRouterError(
                        "relay_error",
                        "Unexpected binary frame on Speko ASR session",
                        retryable=True,
                    )
                event = self._decode_text_event(raw)
                event_type = event.get("type")
                if event_type == "error":
                    error = SpekoRouterError.from_event(event)
                    break
                if event_type == "usage.updated":
                    self.usage = dict(event.get("usage", {}))
                elif event_type == "session.closed":
                    self.usage = dict(event.get("usage", {}))
                    await self.on_event(event)
                    break

                pending = None
                if event_type == "transcript.final":
                    pending = self._answered_commit(event)
                    if pending is not None and pending.abandoned:
                        event = {**event, LATE_COMMIT_CONTEXT: pending.context}
                # Deliver before answering: the finalize must not complete
                # ahead of the final that answers it.
                await self.on_event(event)
                if pending is not None and not pending.answered.done():
                    pending.answered.set_result(None)
        except SpekoRouterError as caught:
            error = caught
        except Exception as caught:  # pragma: no cover - transport-specific
            error = SpekoRouterError("relay_error", str(caught), retryable=True)
        finally:
            self._ready = False
            pending_commits = list(self._pending_commits.values())
            self._pending_commits.clear()
            for pending in pending_commits:
                if not pending.answered.done():
                    pending.answered.set_exception(
                        error
                        or SpekoRouterError(
                            "relay_error",
                            "Speko ASR session closed before final transcript",
                            retryable=True,
                        )
                    )
            if not self._closing or error is not None:
                await self.on_disconnect(error)

    def _answered_commit(self, event: dict[str, Any]) -> _PendingCommit | None:
        """The commit this final answers, or None for the provider's own."""
        commit_id = event.get("commit_id")
        if commit_id is not None:
            self._correlated = True
            # An id that is not pending answers nothing here.
            return self._pending_commits.pop(str(commit_id), None)
        if self._correlated or not self._pending_commits:
            return None
        # A commit's answer came without its id: this Router answers in order.
        self._correlated = False
        return self._pending_commits.pop(next(iter(self._pending_commits)))

    async def _close_socket(self) -> None:
        self._ready = False
        task = self._listener_task
        self._listener_task = None
        if task is not None and task is not asyncio.current_task():
            task.cancel()
            await asyncio.gather(task, return_exceptions=True)
        websocket, self._ws = self._ws, None
        if websocket is not None:
            try:
                await websocket.close()
            except Exception:
                pass

    @staticmethod
    def _decode_text_event(raw: str | bytes) -> dict[str, Any]:
        if not isinstance(raw, str):
            raise SpekoRouterError(
                "relay_error",
                "Expected a JSON text frame from Speko Router",
                retryable=True,
            )
        try:
            event = json.loads(raw)
        except json.JSONDecodeError as error:
            raise SpekoRouterError(
                "relay_error",
                "Speko Router returned invalid JSON",
                retryable=True,
            ) from error
        if not isinstance(event, dict):
            raise SpekoRouterError(
                "relay_error",
                "Speko Router event must be a JSON object",
                retryable=True,
            )
        return event
