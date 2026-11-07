#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""SpatialReal session wrapper: one SDK session at a time, plus reconnects.

Imports ``spatialreal`` only; no TEN. The extension glue owns config,
``ModuleError`` emission and logging, and talks to this module through
``SessionClient``: ``send_audio``/``interrupt`` for the pacer, three
callbacks for state and error reports.

Design D7. Every connect is a new SDK session with a fresh session token
(``expire_at`` now + 1 h) and a fresh Agora token. The reconnect decision
follows the observable event:

* ``start()`` refused: 4001/4010 are fatal, 4003 takes the slow schedule
  (30, 60, 120, 300… s, ten attempts, then fatal), 4503 and transport
  failures the fast schedule (1, 2, 4, 8, 15, 30… s while running);
* ``init()`` refused with HTTP 400: configuration error, stop; HTTP 401 or
  403 (the API key is invalid or revoked): fatal, stop; other token
  failures (5xx, network) take the fast schedule;
* ``on_error`` mid-session: retryable codes and errors with no close code
  take the fast schedule; a 4010 relayed as a server error while the
  session continues is reported without a reconnect; anything else
  non-retryable is fatal;
* ``on_close`` with no preceding error (drain), ``ValueError`` from
  ``send_audio`` (connection gone) and ``transport_frames`` (animation over
  the WebSocket: egress lost the channel) take the fast schedule;
* idle for ``egress_idle_reconnect_ms`` with no request open: reconnect at
  once, before egress's own inactivity timeout drops the channel.

An outage reports ``1001`` once when it begins and ``-1`` if the client
gives up; a planned idle reconnect reports nothing. Design D9: SDK
callbacks are marshalled onto the loop with ``call_soon_threadsafe`` and
bound to the session that registered them, so a callback that arrives
after that session was torn down is dropped.
"""

import asyncio
import logging
import time
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from enum import Enum
from typing import Awaitable, Callable, Optional, Sequence

import spatialreal
from spatialreal import (
    AgoraEgressConfig,
    AvatarSDKError,
    SessionTokenError,
    new_avatar_session,
)

from .agora_token import build_agora_token

logger = logging.getLogger(__name__)

FAST_BACKOFF_S: Sequence[float] = (1, 2, 4, 8, 15, 30)
SLOW_BACKOFF_S: Sequence[float] = (30, 60, 120, 300)
SLOW_MAX_ATTEMPTS = 10
SESSION_TOKEN_TTL_S = 3600
EGRESS_IDLE_RECONNECT_MS = 1_080_000
# How often the idle timer re-checks while a request is still open.
IDLE_RECHECK_S = 1.0

CODE_CONFIG = -1012
CODE_FATAL = -1
CODE_NON_FATAL = 1001

_CLOSE_CREDITS = 4001
_CLOSE_CONCURRENCY = 4003
_CLOSE_AUTH = 4010
# Token exchange (console API) statuses that retrying cannot change.
_TOKEN_BAD_REQUEST = 400
_TOKEN_REFUSED = (401, 403)


@dataclass
class SessionSettings:
    """Everything a connect needs; validated by the glue before it gets here."""

    api_key: str
    avatar_id: str
    app_id: str = ""
    environment: str = "us-west"
    console_endpoint_url: str = ""
    ingress_endpoint_url: str = ""
    sample_rate: int = 24000
    channel: str = ""
    uid: int = 0
    agora_appid: str = ""
    agora_appcert: str = ""
    agora_token: str = ""
    egress_idle_reconnect_ms: int = EGRESS_IDLE_RECONNECT_MS
    source_timeline_only: bool = False


@dataclass
class ErrorReport:
    """What the glue turns into a ``ModuleError`` (D8)."""

    code: int  # CODE_FATAL | CODE_NON_FATAL | CODE_CONFIG
    # WS close code or token-exchange HTTP status as a string, or the
    # exception class name.
    vendor_code: str
    message: str
    session_id: str = ""


class Decision(Enum):
    FAST = "fast"
    SLOW = "slow"
    STOP = "stop"
    REPORT_ONLY = "report_only"
    CONFIG = "config"


def _vendor_code(exc: Exception) -> str:
    close_code = getattr(exc, "close_code", None)
    if close_code is not None:
        return str(close_code)
    if isinstance(exc, SessionTokenError) and exc.http_status is not None:
        return str(exc.http_status)
    return type(exc).__name__


def classify_start_error(
    exc: Exception, *, source_timeline_only: bool = False
) -> tuple[Decision, int, str]:
    """Decide on an exception from ``init()`` or ``start()`` (D7)."""
    vendor = _vendor_code(exc)
    unsupported_config = getattr(
        spatialreal, "UnsupportedConfigurationError", None
    )
    if (
        source_timeline_only
        and unsupported_config is not None
        and isinstance(exc, unsupported_config)
    ):
        return Decision.CONFIG, CODE_CONFIG, vendor
    if isinstance(exc, SessionTokenError):
        if exc.http_status == _TOKEN_BAD_REQUEST:
            return Decision.CONFIG, CODE_CONFIG, vendor
        if exc.http_status in _TOKEN_REFUSED:
            # The console refused the API key: no session will ever be
            # minted with it, so the 4010 handling downstream never runs.
            return Decision.STOP, CODE_FATAL, vendor
        return Decision.FAST, CODE_NON_FATAL, vendor
    if isinstance(exc, AvatarSDKError):
        if exc.close_code is None:
            # No close code: transport failure, a frame that did not parse,
            # or a ServerError with no session-fate code. D7 says fast.
            return Decision.FAST, CODE_NON_FATAL, vendor
        if exc.close_code in (_CLOSE_CREDITS, _CLOSE_AUTH):
            return Decision.STOP, CODE_FATAL, vendor
        if exc.close_code == _CLOSE_CONCURRENCY:
            return Decision.SLOW, CODE_NON_FATAL, vendor
        if exc.retryable:
            return Decision.FAST, CODE_NON_FATAL, vendor
        return Decision.STOP, CODE_FATAL, vendor
    if isinstance(exc, ValueError):
        # The SDK raises ValueError for a request it cannot build (missing
        # key, bad endpoint scheme): retrying changes nothing.
        return Decision.CONFIG, CODE_CONFIG, vendor
    return Decision.FAST, CODE_NON_FATAL, vendor


def classify_runtime_error(exc: Exception) -> tuple[Decision, int, str]:
    """Decide on an exception from ``on_error`` or a send (D7)."""
    vendor = _vendor_code(exc)
    if isinstance(exc, AvatarSDKError):
        if exc.close_code is None or exc.retryable:
            # D7: retryable codes and the absence of a close code (a failed
            # inference call relayed as ServerError{code: 0}, a frame that
            # did not parse, a transport drop) both take the fast schedule.
            return Decision.FAST, CODE_NON_FATAL, vendor
        if exc.close_code == _CLOSE_AUTH and exc.server_code:
            # A server error frame, not a close: inference auth failed but
            # the session continues.
            return Decision.REPORT_ONLY, CODE_NON_FATAL, vendor
        return Decision.STOP, CODE_FATAL, vendor
    return Decision.FAST, CODE_NON_FATAL, vendor


class SessionClient:
    """Keeps one SpatialReal session up for the lifetime of the extension.

    All methods run on one asyncio loop. ``start()`` spawns the connect
    loop and returns; ``stop()`` ends everything. ``connected`` and
    ``connection_id`` are readable at any time.
    """

    def __init__(
        self,
        settings: SessionSettings,
        *,
        on_report: Callable[[ErrorReport], None],
        on_connected: Callable[[str], None],
        on_disconnected: Callable[[], None],
        session_factory: Callable[..., object] = new_avatar_session,
        clock: Callable[[], float] = time.monotonic,
        sleep: Callable[[float], Awaitable[None]] = asyncio.sleep,
        fast_backoff: Sequence[float] = FAST_BACKOFF_S,
        slow_backoff: Sequence[float] = SLOW_BACKOFF_S,
        slow_max_attempts: int = SLOW_MAX_ATTEMPTS,
    ) -> None:
        self._settings = settings
        self._on_report = on_report
        self._on_connected = on_connected
        self._on_disconnected = on_disconnected
        self._factory = session_factory
        self._clock = clock
        self._sleep = sleep
        self._fast_backoff = tuple(fast_backoff)
        self._slow_backoff = tuple(slow_backoff)
        self._slow_max_attempts = slow_max_attempts

        self.connected = False
        self.connection_id = ""

        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._run_task: Optional[asyncio.Task] = None
        self._idle_task: Optional[asyncio.Task] = None
        self._session = None
        # Incremented per connect attempt and again at teardown; callbacks
        # carry the value they were registered with.
        self._gen = 0
        self._closing = False
        self._stopped = False
        self._wake = asyncio.Event()
        self._pending: Optional[tuple[Decision, bool]] = None
        self._last_vendor_code = ""
        self._outage_reported = False
        self._fast_index = 0
        self._slow_attempts = 0
        self._last_rpc_at = clock()
        self._request_open = False

    # -- lifecycle ----------------------------------------------------------

    async def start(self) -> None:
        """Spawn the connect loop; returns immediately."""
        if self._run_task is not None or self._stopped:
            return
        self._loop = asyncio.get_running_loop()
        self._run_task = self._loop.create_task(self._run())

    async def stop(self) -> None:
        """End reconnection, cancel the loop and timer, close the session."""
        self._stopped = True
        self._closing = True
        task, self._run_task = self._run_task, None
        if task is not None and task is not asyncio.current_task():
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass
        self._cancel_idle_timer()
        session, self._session = self._session, None
        self._mark_disconnected()
        if session is not None:
            await self._close_quietly(session)

    def note_request_state(self, open_: bool) -> None:
        """The idle timer only reconnects while no request is open."""
        self._request_open = open_

    # -- audio --------------------------------------------------------------

    async def send_audio(self, pcm: bytes, end: bool = False) -> bool:
        """Forward one chunk; False when not connected or the send failed."""
        session = self._session
        if not self.connected or session is None:
            return False
        try:
            await session.send_audio(pcm, end=end)
        except ValueError as exc:
            # The SDK's "connection is not established": the socket is gone
            # and no callback may have told us yet.
            self._request_reconnect(Decision.FAST, _vendor_code(exc), str(exc))
            return False
        except AvatarSDKError as exc:
            self._handle_runtime_exception(exc)
            return False
        self._last_rpc_at = self._clock()
        return True

    async def interrupt(self) -> bool:
        """Interrupt the last request; False when there was none to cut."""
        session = self._session
        if not self.connected or session is None:
            return False
        try:
            await session.interrupt()
        except ValueError:
            # "no request to interrupt" is benign; a lost connection also
            # raises ValueError here but surfaces through on_close or the
            # next send, so it is not treated as a drop.
            return False
        except AvatarSDKError as exc:
            self._handle_runtime_exception(exc)
            return False
        self._last_rpc_at = self._clock()
        return True

    # -- connect loop -------------------------------------------------------

    async def _run(self) -> None:
        while not self._stopped:
            self._wake.clear()
            connected = await self._connect_once()
            if connected:
                await self._wake.wait()
                await self._teardown()
            if self._stopped:
                return
            decision, immediate = self._pending or (Decision.FAST, False)
            self._pending = None
            delay = 0.0 if immediate else self._next_delay(decision)
            if delay is None:
                self._report(
                    CODE_FATAL,
                    self._last_vendor_code,
                    "gave up after %d reconnect attempts"
                    % self._slow_max_attempts,
                )
                self._stopped = True
                return
            if delay > 0:
                await self._sleep(delay)

    async def _connect_once(self) -> bool:
        s = self._settings
        self._gen += 1
        gen = self._gen
        loop = self._loop
        session = None
        try:
            expire_at = datetime.now(timezone.utc) + timedelta(
                seconds=SESSION_TOKEN_TTL_S
            )
            egress_options = {}
            if s.source_timeline_only:
                has_source_field = "source_timeline_only" in getattr(
                    AgoraEgressConfig, "__dataclass_fields__", {}
                )
                has_ack_check = (
                    getattr(spatialreal, "UnsupportedConfigurationError", None)
                    is not None
                )
                if not (has_source_field and has_ack_check):
                    raise ValueError(
                        "params.source_timeline_only requires a SpatialReal "
                        "SDK with source-timeline-only handshake support"
                    )
                egress_options["source_timeline_only"] = True
            egress = AgoraEgressConfig(
                channel_name=s.channel,
                token=build_agora_token(
                    s.agora_appid,
                    s.agora_appcert,
                    s.channel,
                    s.uid,
                    explicit_token=s.agora_token,
                ),
                uid=s.uid,
                **egress_options,
            )
            kwargs = {
                "api_key": s.api_key,
                "avatar_id": s.avatar_id,
                "expire_at": expire_at,
                "sample_rate": s.sample_rate,
                "environment": s.environment,
                "agora_egress": egress,
                "on_error": lambda exc: loop.call_soon_threadsafe(
                    self._handle_error, gen, exc
                ),
                "on_close": lambda: loop.call_soon_threadsafe(
                    self._handle_close, gen
                ),
                "transport_frames": lambda payload, end: (
                    loop.call_soon_threadsafe(
                        self._handle_transport_frames, gen
                    )
                ),
            }
            if s.app_id:
                kwargs["app_id"] = s.app_id
            if s.console_endpoint_url:
                kwargs["console_endpoint_url"] = s.console_endpoint_url
            if s.ingress_endpoint_url:
                kwargs["ingress_endpoint_url"] = s.ingress_endpoint_url
            session = self._factory(**kwargs)
            self._session = session
            await session.init()
            connection_id = await session.start()
        except Exception as exc:
            self._session = None
            decision, code, vendor = classify_start_error(
                exc, source_timeline_only=s.source_timeline_only
            )
            logger.warning(
                "spatialreal connect failed (%s, %s): %s",
                vendor,
                decision.value,
                exc,
            )
            self._last_vendor_code = vendor
            if decision in (Decision.STOP, Decision.CONFIG):
                self._report(code, vendor, str(exc))
                self._stopped = True
            else:
                self._pending = (decision, False)
                self._report_outage(vendor, str(exc))
            return False

        self.connected = True
        self.connection_id = connection_id or ""
        self._fast_index = 0
        self._slow_attempts = 0
        self._outage_reported = False
        self._last_rpc_at = self._clock()
        self._start_idle_timer(gen)
        self._on_connected(self.connection_id)
        return True

    def _next_delay(self, decision: Decision) -> Optional[float]:
        if decision is Decision.SLOW:
            self._slow_attempts += 1
            if self._slow_attempts > self._slow_max_attempts:
                return None
            index = min(self._slow_attempts - 1, len(self._slow_backoff) - 1)
            return self._slow_backoff[index]
        index = min(self._fast_index, len(self._fast_backoff) - 1)
        self._fast_index += 1
        return self._fast_backoff[index]

    async def _teardown(self) -> None:
        """Close the current session; stale callbacks are dropped afterwards."""
        self._cancel_idle_timer()
        session = self._session
        if session is None:
            return
        self._closing = True
        try:
            await self._close_quietly(session)
        finally:
            self._closing = False
            self._gen += 1
            if self._session is session:
                self._session = None

    @staticmethod
    async def _close_quietly(session) -> None:
        try:
            await session.close()
        except Exception:
            logger.exception("spatialreal session close raised")

    # -- events -------------------------------------------------------------

    def _stale(self, gen: int) -> bool:
        return (
            gen != self._gen
            or self._closing
            or self._stopped
            or self._session is None
        )

    def _handle_error(self, gen: int, exc: Exception) -> None:
        if self._stale(gen):
            return
        self._handle_runtime_exception(exc)

    def _handle_runtime_exception(self, exc: Exception) -> None:
        decision, code, vendor = classify_runtime_error(exc)
        logger.warning(
            "spatialreal session error (%s, %s): %s",
            vendor,
            decision.value,
            exc,
        )
        if decision is Decision.REPORT_ONLY:
            self._report(code, vendor, str(exc))
        elif decision is Decision.STOP:
            self._fail_fatally(vendor, str(exc))
        else:
            self._request_reconnect(Decision.FAST, vendor, str(exc))

    def _handle_close(self, gen: int) -> None:
        if self._stale(gen):
            return
        self._request_reconnect(
            Decision.FAST, "on_close", "session closed by the server"
        )

    def _handle_transport_frames(self, gen: int) -> None:
        if self._stale(gen):
            return
        self._request_reconnect(
            Decision.FAST,
            "transport_frames",
            "animation arrived over the WebSocket: egress left the channel",
        )

    def _request_reconnect(
        self,
        decision: Decision,
        vendor_code: str,
        message: str,
        *,
        immediate: bool = False,
        report: bool = True,
    ) -> None:
        """Tear down and reconnect; a no-op unless currently connected, so
        the first event of an outage wins and the rest are ignored."""
        if self._stopped or not self.connected:
            return
        self._pending = (decision, immediate)
        self._last_vendor_code = vendor_code
        if report:
            self._report_outage(vendor_code, message)
        self._mark_disconnected()
        self._wake.set()

    def _fail_fatally(self, vendor_code: str, message: str) -> None:
        if self._stopped:
            return
        self._report(CODE_FATAL, vendor_code, message)
        self._stopped = True
        self._pending = (Decision.STOP, False)
        self._mark_disconnected()
        self._wake.set()

    def _mark_disconnected(self) -> None:
        if not self.connected:
            return
        self.connected = False
        self.connection_id = ""
        self._on_disconnected()

    # -- reports ------------------------------------------------------------

    def _report(self, code: int, vendor_code: str, message: str) -> None:
        self._on_report(
            ErrorReport(code, vendor_code, message, self.connection_id)
        )

    def _report_outage(self, vendor_code: str, message: str) -> None:
        """Report 1001 once per outage; later attempts stay quiet."""
        if self._outage_reported:
            return
        self._outage_reported = True
        self._report(CODE_NON_FATAL, vendor_code, message)

    # -- idle timer ---------------------------------------------------------

    def _start_idle_timer(self, gen: int) -> None:
        self._cancel_idle_timer()
        if self._settings.egress_idle_reconnect_ms <= 0 or self._loop is None:
            return
        self._idle_task = self._loop.create_task(self._idle_loop(gen))

    def _cancel_idle_timer(self) -> None:
        task, self._idle_task = self._idle_task, None
        if task is not None and task is not asyncio.current_task():
            task.cancel()

    async def _idle_loop(self, gen: int) -> None:
        idle_s = self._settings.egress_idle_reconnect_ms / 1000.0
        while not self._stale(gen):
            remaining = idle_s - (self._clock() - self._last_rpc_at)
            if remaining > 0:
                await self._sleep(remaining)
                continue
            if self._request_open:
                await self._sleep(IDLE_RECHECK_S)
                continue
            logger.info(
                "spatialreal session idle for %.0f s: reconnecting before "
                "egress drops the channel",
                idle_s,
            )
            self._request_reconnect(
                Decision.FAST,
                "idle",
                "planned idle reconnect",
                immediate=True,
                report=False,
            )
            return
