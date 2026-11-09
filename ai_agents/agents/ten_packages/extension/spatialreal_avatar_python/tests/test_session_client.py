#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Unit tests for the SDK session wrapper and its reconnect policy (D7).

The SDK is replaced by ``FakeSession`` objects a ``FakeFactory`` hands out
through ``session_factory``; each can fail ``init``/``start``, record what
was sent, and fire the SDK callbacks the way the real session does. Time is
virtual: the client's ``sleep`` parks on a future that ``VirtualTime.advance``
resolves, so a backoff schedule is asserted by advancing exactly the delays
the schedule promises.
"""

import asyncio
from datetime import datetime, timedelta, timezone

import pytest
from spatialreal import AvatarSDKError, AvatarSDKErrorCode, SessionTokenError
from spatialreal import UnsupportedConfigurationError

from spatialreal_avatar_python import session_client
from spatialreal_avatar_python.session_client import (
    CODE_CONFIG,
    CODE_FATAL,
    CODE_NON_FATAL,
    Decision,
    ErrorReport,
    SessionClient,
    SessionSettings,
    classify_runtime_error,
    classify_start_error,
)

# Test literals, not credentials. The App ID has the width the token builder
# expects.
APP_ID = "a" * 32
PCM = b"\x00\x00" * 240


async def settle(rounds: int = 50) -> None:
    """Let every ready callback and task step run."""
    for _ in range(rounds):
        await asyncio.sleep(0)


class VirtualTime:
    """A clock plus a ``sleep`` that only returns when the test advances."""

    def __init__(self, start_s: float = 1000.0):
        self._now_s = start_s
        self._sleepers: list[tuple[float, asyncio.Future]] = []
        self.requested: list[float] = []

    def now(self) -> float:
        return self._now_s

    async def sleep(self, seconds: float) -> None:
        self.requested.append(seconds)
        fut = asyncio.get_running_loop().create_future()
        entry = (self._now_s + seconds, fut)
        self._sleepers.append(entry)
        try:
            await fut
        finally:
            if entry in self._sleepers:
                self._sleepers.remove(entry)

    @property
    def pending(self) -> list[float]:
        """Remaining time of every sleep still parked, in request order."""
        return [deadline - self._now_s for deadline, _ in self._sleepers]

    async def advance(self, seconds: float) -> None:
        target = self._now_s + seconds
        while True:
            due = sorted(
                (e for e in self._sleepers if e[0] <= target),
                key=lambda e: e[0],
            )
            if not due:
                break
            deadline, fut = due[0]
            self._sleepers.remove(due[0])
            self._now_s = max(self._now_s, deadline)
            if not fut.done():
                fut.set_result(None)
            await settle()
        self._now_s = target
        await settle()


def sdk_error(close_code, *, server_code=None, code=None) -> AvatarSDKError:
    if code is None:
        code = (
            AvatarSDKErrorCode.connectionClosed
            if close_code is None
            else AvatarSDKErrorCode.unknown
        )
    return AvatarSDKError(
        code,
        f"test error close_code={close_code}",
        close_code=close_code,
        server_code=server_code,
    )


class FakeSession:
    """Stands in for ``spatialreal.AvatarSession``.

    ``close()`` fires ``on_close`` once, as the SDK does after our own close;
    the tests fire ``on_error``/``on_close``/``transport_frames`` by hand for
    everything the server would do.
    """

    def __init__(self, index: int, kwargs: dict, init_exc, start_exc):
        self.index = index
        self.kwargs = kwargs
        self.init_exc = init_exc
        self.start_exc = start_exc
        self.connection_id = f"conn-{index}"
        self.capabilities = (
            "agora_source_timeline_only",
            "agora_source_burst_v1",
        )
        self.sends: list[tuple[bytes, bool]] = []
        self.interrupts = 0
        self.closes = 0
        self.send_exc: Exception | None = None
        self.interrupt_exc: Exception | None = None
        self._close_notified = False

    async def init(self) -> None:
        if self.init_exc is not None:
            raise self.init_exc

    async def start(self) -> str:
        if self.start_exc is not None:
            raise self.start_exc
        return self.connection_id

    async def send_audio(self, audio: bytes, end: bool = False) -> str:
        if self.send_exc is not None:
            raise self.send_exc
        self.sends.append((audio, end))
        return "req"

    async def interrupt(self) -> str:
        if self.interrupt_exc is not None:
            raise self.interrupt_exc
        self.interrupts += 1
        return "req"

    async def close(self) -> None:
        self.closes += 1
        self.fire_close()

    def fire_close(self) -> None:
        if not self._close_notified:
            self._close_notified = True
            self.kwargs["on_close"]()

    def fire_error(self, exc: Exception) -> None:
        self.kwargs["on_error"](exc)

    def fire_transport_frames(self) -> None:
        self.kwargs["transport_frames"](b"\x00", False)


class FakeFactory:
    """Builds one ``FakeSession`` per connect attempt, failing on request."""

    def __init__(self, *, init_error=None, start_error=None):
        self.sessions: list[FakeSession] = []
        self._init_error = init_error or (lambda i: None)
        self._start_error = start_error or (lambda i: None)

    def __call__(self, **kwargs) -> FakeSession:
        i = len(self.sessions)
        session = FakeSession(
            i, kwargs, self._init_error(i), self._start_error(i)
        )
        self.sessions.append(session)
        return session

    @property
    def last(self) -> FakeSession:
        return self.sessions[-1]


class Harness:
    def __init__(self, factory: FakeFactory, vt: VirtualTime, **overrides):
        settings = dict(
            api_key="k",
            avatar_id="av",
            channel="room",
            uid=12345,
            agora_appid=APP_ID,
        )
        settings.update(overrides)
        self.factory = factory
        self.vt = vt
        self.reports: list[ErrorReport] = []
        self.connected_ids: list[str] = []
        self.disconnects = 0
        self.client = SessionClient(
            SessionSettings(**settings),
            on_report=self.reports.append,
            on_connected=self.connected_ids.append,
            on_disconnected=self._on_disconnected,
            session_factory=factory,
            clock=vt.now,
            sleep=vt.sleep,
        )

    def _on_disconnected(self) -> None:
        self.disconnects += 1

    @property
    def codes(self) -> list[tuple[int, str]]:
        return [(r.code, r.vendor_code) for r in self.reports]


class BurstFactory(FakeFactory):
    def __init__(self, capabilities, **kwargs):
        super().__init__(**kwargs)
        self.capabilities = capabilities

    def __call__(self, **kwargs):
        session = super().__call__(**kwargs)
        session.capabilities = tuple(self.capabilities(session.index))
        return session


@pytest.fixture
def vt() -> VirtualTime:
    return VirtualTime()


async def connected_harness(vt: VirtualTime, **overrides) -> Harness:
    h = Harness(FakeFactory(), vt, **overrides)
    await h.client.start()
    await settle()
    assert h.client.connected
    return h


# -- classification -----------------------------------------------------------


@pytest.mark.parametrize(
    "exc, expected",
    [
        (sdk_error(4001), (Decision.STOP, CODE_FATAL, "4001")),
        (sdk_error(4010), (Decision.STOP, CODE_FATAL, "4010")),
        (sdk_error(4003), (Decision.SLOW, CODE_NON_FATAL, "4003")),
        (sdk_error(4503), (Decision.FAST, CODE_NON_FATAL, "4503")),
        (sdk_error(1008), (Decision.STOP, CODE_FATAL, "1008")),
        (sdk_error(None), (Decision.FAST, CODE_NON_FATAL, "AvatarSDKError")),
        (
            # Handshake frame that did not parse: no close code, SDK says
            # not retryable, D7 says fast.
            sdk_error(None, code=AvatarSDKErrorCode.protocolError),
            (Decision.FAST, CODE_NON_FATAL, "AvatarSDKError"),
        ),
        (
            SessionTokenError("bad request", http_status=400),
            (Decision.CONFIG, CODE_CONFIG, "400"),
        ),
        (
            SessionTokenError("unauthorized", http_status=401),
            (Decision.STOP, CODE_FATAL, "401"),
        ),
        (
            SessionTokenError("forbidden", http_status=403),
            (Decision.STOP, CODE_FATAL, "403"),
        ),
        (
            SessionTokenError("unavailable", http_status=503),
            (Decision.FAST, CODE_NON_FATAL, "503"),
        ),
        (
            SessionTokenError("unavailable", http_status=502),
            (Decision.FAST, CODE_NON_FATAL, "502"),
        ),
        (
            SessionTokenError(
                "timeout", code=AvatarSDKErrorCode.connectionFailed
            ),
            (Decision.FAST, CODE_NON_FATAL, "SessionTokenError"),
        ),
        (OSError("dns"), (Decision.FAST, CODE_NON_FATAL, "OSError")),
    ],
)
def test_classify_start_errors(exc, expected):
    assert classify_start_error(exc) == expected


@pytest.mark.parametrize(
    "exc, expected",
    [
        (sdk_error(4002), (Decision.FAST, CODE_NON_FATAL, "4002")),
        (sdk_error(4503), (Decision.FAST, CODE_NON_FATAL, "4503")),
        (sdk_error(None), (Decision.FAST, CODE_NON_FATAL, "AvatarSDKError")),
        (
            # One inference call failed: ServerError{code: 0}, session
            # continues; the SDK marks it not retryable, D7 says fast.
            sdk_error(
                None, code=AvatarSDKErrorCode.serverError, server_code="0"
            ),
            (Decision.FAST, CODE_NON_FATAL, "AvatarSDKError"),
        ),
        (
            sdk_error(None, code=AvatarSDKErrorCode.protocolError),
            (Decision.FAST, CODE_NON_FATAL, "AvatarSDKError"),
        ),
        (
            sdk_error(4010, server_code="4010"),
            (Decision.REPORT_ONLY, CODE_NON_FATAL, "4010"),
        ),
        (sdk_error(4010), (Decision.STOP, CODE_FATAL, "4010")),
        (sdk_error(4001), (Decision.STOP, CODE_FATAL, "4001")),
        (RuntimeError("boom"), (Decision.FAST, CODE_NON_FATAL, "RuntimeError")),
    ],
)
def test_classify_runtime_errors(exc, expected):
    assert classify_runtime_error(exc) == expected


# -- connect ------------------------------------------------------------------


@pytest.mark.asyncio
async def test_connect_success_reports_connected_and_sends(vt):
    factory = FakeFactory()
    h = Harness(
        factory,
        vt,
        app_id="app",
        console_endpoint_url="https://console.test",
        ingress_endpoint_url="wss://ingress.test/v2",
        sample_rate=16000,
    )
    assert not await h.client.send_audio(PCM)

    await h.client.start()
    await settle()

    assert h.client.connected
    assert h.client.connection_id == "conn-0"
    assert h.connected_ids == ["conn-0"]
    assert h.reports == []
    kwargs = factory.last.kwargs
    assert kwargs["api_key"] == "k"
    assert kwargs["avatar_id"] == "av"
    assert kwargs["app_id"] == "app"
    assert kwargs["sample_rate"] == 16000
    assert kwargs["console_endpoint_url"] == "https://console.test"
    assert kwargs["ingress_endpoint_url"] == "wss://ingress.test/v2"
    egress = kwargs["agora_egress"]
    assert (egress.channel_name, egress.uid) == ("room", 12345)
    # No certificate: the App ID itself is the token (D3).
    assert egress.token == APP_ID
    assert egress.source_timeline_only is True
    assert egress.source_playback_profile == 1

    assert await h.client.send_audio(PCM)
    assert await h.client.send_audio(b"", end=True)
    assert factory.last.sends == [(PCM, False), (b"", True)]
    assert await h.client.interrupt()
    assert factory.last.interrupts == 1


@pytest.mark.asyncio
async def test_optional_settings_are_omitted_when_empty(vt):
    h = await connected_harness(vt)
    kwargs = h.factory.last.kwargs
    assert "app_id" not in kwargs
    assert "console_endpoint_url" not in kwargs
    assert "ingress_endpoint_url" not in kwargs
    assert kwargs["environment"] == "us-west"


@pytest.mark.asyncio
async def test_source_timeline_mode_survives_reconnect(vt):
    h = await connected_harness(vt, source_timeline_only=True)
    first = h.factory.last
    assert first.kwargs["agora_egress"].source_timeline_only is True
    first.fire_close()
    await settle()
    await vt.advance(1)
    assert h.client.connected
    assert len(h.factory.sessions) == 2
    assert h.factory.last.kwargs["agora_egress"].source_timeline_only is True


@pytest.mark.asyncio
async def test_source_timeline_missing_ack_is_config_error_without_retry(vt):
    factory = FakeFactory(
        start_error=lambda _: UnsupportedConfigurationError("required mode")
    )
    h = Harness(factory, vt, source_timeline_only=True)
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "UnsupportedConfigurationError")]
    assert not h.client.connected
    assert vt.pending == []
    await vt.advance(300)
    assert len(factory.sessions) == 1


@pytest.mark.asyncio
async def test_source_timeline_old_sdk_is_explicit_config_error(
    vt, monkeypatch
):
    class LegacyEgressConfig:
        __dataclass_fields__ = {}

        def __init__(self, **kwargs):
            self.__dict__.update(kwargs)

    monkeypatch.setattr(session_client, "AgoraEgressConfig", LegacyEgressConfig)
    factory = FakeFactory()
    h = Harness(factory, vt, source_timeline_only=True)
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "ValueError")]
    assert not h.client.connected
    assert factory.sessions == []
    assert vt.pending == []


@pytest.mark.asyncio
async def test_old_sdk_still_connects_when_source_mode_is_disabled(
    vt, monkeypatch
):
    class LegacyEgressConfig:
        def __init__(self, **kwargs):
            assert "source_timeline_only" not in kwargs
            self.__dict__.update(kwargs)

    monkeypatch.setattr(session_client, "AgoraEgressConfig", LegacyEgressConfig)
    h = await connected_harness(vt, source_timeline_only=False)
    assert h.factory.last.kwargs["agora_egress"].channel_name == "room"
    assert h.reports == []


@pytest.mark.asyncio
async def test_source_timeline_requires_sdk_ack_support_not_just_field(
    vt, monkeypatch
):
    monkeypatch.delattr(
        session_client.spatialreal, "UnsupportedConfigurationError"
    )
    factory = FakeFactory()
    h = Harness(factory, vt, source_timeline_only=True)
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "ValueError")]
    assert factory.sessions == []
    assert vt.pending == []


BURST_CAPABILITIES = (
    "agora_source_timeline_only",
    "agora_source_burst_v1",
)


@pytest.mark.asyncio
async def test_burst_profile_reconnect_rechecks_real_confirmation(vt):
    factory = BurstFactory(lambda i: BURST_CAPABILITIES if i == 0 else ())
    h = Harness(
        factory, vt, source_timeline_only=True, source_playback_profile=1
    )
    await h.client.start()
    await settle()
    assert h.client.connected
    assert factory.last.kwargs["agora_egress"].source_playback_profile == 1
    assert await h.client.send_audio(PCM)
    first = factory.last
    first.fire_close()
    await settle()
    await vt.advance(1)
    assert not h.client.connected
    assert h.connected_ids == [first.connection_id]
    assert len(factory.sessions) == 2
    assert factory.last.kwargs["agora_egress"].source_playback_profile == 1
    assert factory.last.closes == 1
    assert not await h.client.send_audio(PCM)
    assert factory.last.sends == []
    assert vt.pending == []
    await h.client.stop()


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "capabilities", [(), BURST_CAPABILITIES[:1], BURST_CAPABILITIES[1:]]
)
async def test_burst_post_start_refusal_closes_and_invalidates_callbacks(
    vt, capabilities
):
    # Observe retained ownership and callback invalidation during awaited close.
    # pylint: disable=protected-access
    class ClosingFactory(BurstFactory):
        def __call__(self, **kwargs):
            session = super().__call__(**kwargs)
            original_close = session.close
            generation = h.client._gen

            async def close():
                assert h.client._session is session
                assert h.client._stale(generation)
                session.fire_error(sdk_error(4010))
                await asyncio.sleep(0)
                await original_close()

            session.close = close
            return session

    # Capture the generation at creation, then observe ownership while close
    # yields: callbacks must already be stale before the session is cleared.
    factory = ClosingFactory(lambda _: capabilities)
    h = Harness(
        factory, vt, source_timeline_only=True, source_playback_profile=1
    )
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "UnsupportedConfigurationError")]
    assert not h.client.connected
    assert h.connected_ids == []
    assert h.client._session is None
    assert factory.last.closes == 1
    assert factory.last.sends == []
    factory.last.fire_error(sdk_error(4503))
    factory.last.fire_transport_frames()
    await settle()
    await vt.advance(300)
    assert len(factory.sessions) == 1
    assert len(h.reports) == 1
    assert vt.pending == []


@pytest.mark.asyncio
@pytest.mark.parametrize("capabilities", [None, " ".join(BURST_CAPABILITIES)])
async def test_burst_missing_or_malformed_capabilities_is_config_error(
    vt, capabilities
):
    class UnconfirmedFactory(FakeFactory):
        def __call__(self, **kwargs):
            session = super().__call__(**kwargs)
            session.capabilities = capabilities
            return session

    factory = UnconfirmedFactory()
    h = Harness(
        factory, vt, source_timeline_only=True, source_playback_profile=1
    )
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "UnsupportedConfigurationError")]
    assert factory.last.closes == 1
    assert factory.last.sends == []
    assert vt.pending == []


@pytest.mark.asyncio
async def test_burst_old_sdk_rejected_before_session_creation(vt, monkeypatch):
    monkeypatch.delattr(session_client.spatialreal, "RtcSourcePlaybackProfile")
    factory = FakeFactory()
    h = Harness(
        factory, vt, source_timeline_only=True, source_playback_profile=1
    )
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_CONFIG, "ValueError")]
    assert factory.sessions == []
    assert vt.pending == []


@pytest.mark.asyncio
async def test_zero_profile_omits_new_keyword_for_old_sdk(vt, monkeypatch):
    class LegacyEgressConfig:
        def __init__(self, **kwargs):
            assert "source_playback_profile" not in kwargs
            self.__dict__.update(kwargs)

    monkeypatch.setattr(session_client, "AgoraEgressConfig", LegacyEgressConfig)
    monkeypatch.delattr(session_client.spatialreal, "RtcSourcePlaybackProfile")
    h = await connected_harness(
        vt, source_timeline_only=False, source_playback_profile=0
    )
    assert h.reports == []
    await h.client.stop()


@pytest.mark.asyncio
@pytest.mark.parametrize("close_code, retries", [(1008, False), (4503, True)])
async def test_burst_refusal_does_not_hide_transient_failure(
    vt, close_code, retries
):
    factory = BurstFactory(
        lambda _: BURST_CAPABILITIES,
        start_error=lambda _: sdk_error(close_code),
    )
    h = Harness(
        factory, vt, source_timeline_only=True, source_playback_profile=1
    )
    await h.client.start()
    await settle()
    assert factory.last.closes == 1
    assert factory.last.sends == []
    assert vt.pending == ([1] if retries else [])
    assert h.codes == [
        (CODE_NON_FATAL if retries else CODE_FATAL, str(close_code))
    ]
    await h.client.stop()


@pytest.mark.asyncio
async def test_source_timeline_parse_failure_keeps_fast_retry(vt):
    factory = FakeFactory(
        start_error=lambda _: AvatarSDKError(
            AvatarSDKErrorCode.protocolError,
            "malformed protobuf",
            phase="websocket_handshake",
        )
    )
    h = Harness(factory, vt, source_timeline_only=True)
    await h.client.start()
    await settle()
    assert h.codes == [(CODE_NON_FATAL, "AvatarSDKError")]
    assert vt.pending == [1]


@pytest.mark.asyncio
async def test_start_4003_uses_slow_schedule_then_stops(vt):
    factory = FakeFactory(start_error=lambda i: sdk_error(4003))
    h = Harness(factory, vt)
    await h.client.start()
    await settle()

    assert len(factory.sessions) == 1
    assert h.codes == [(CODE_NON_FATAL, "4003")]
    assert not h.client.connected

    for attempt, delay in enumerate(
        [30, 60, 120, 300, 300, 300, 300, 300, 300, 300], start=1
    ):
        assert vt.pending == [delay], f"attempt {attempt}"
        await vt.advance(delay)
        assert len(factory.sessions) == attempt + 1

    # Ten retries failed: one fatal report, and the client is stopped.
    assert vt.pending == []
    assert h.codes == [(CODE_NON_FATAL, "4003"), (CODE_FATAL, "4003")]
    await vt.advance(3600)
    assert len(factory.sessions) == 11


@pytest.mark.asyncio
async def test_start_4503_uses_fast_schedule_until_it_connects(vt):
    factory = FakeFactory(
        start_error=lambda i: sdk_error(4503) if i < 3 else None
    )
    h = Harness(factory, vt)
    await h.client.start()
    await settle()

    assert h.codes == [(CODE_NON_FATAL, "4503")]
    for delay in (1, 2, 4):
        assert vt.pending == [delay]
        await vt.advance(delay)
    assert h.client.connected
    assert h.connected_ids == ["conn-3"]
    # One outage, one non-fatal report.
    assert h.codes == [(CODE_NON_FATAL, "4503")]


@pytest.mark.asyncio
async def test_init_400_is_a_config_error_and_stops(vt):
    factory = FakeFactory(
        init_error=lambda i: SessionTokenError("skew", http_status=400)
    )
    h = Harness(factory, vt)
    await h.client.start()
    await settle()

    assert h.codes == [(CODE_CONFIG, "400")]
    assert vt.pending == []
    await vt.advance(3600)
    assert len(factory.sessions) == 1


@pytest.mark.asyncio
@pytest.mark.parametrize("status", [401, 403])
async def test_init_401_and_403_are_fatal_and_stop(vt, status):
    """An invalid or revoked API key fails the token exchange before any
    session exists: one fatal report, no retry, no 4010 to wait for."""
    factory = FakeFactory(
        init_error=lambda i: SessionTokenError(
            "api key refused", http_status=status
        )
    )
    h = Harness(factory, vt)
    await h.client.start()
    await settle()

    assert h.codes == [(CODE_FATAL, str(status))]
    assert h.reports[0].session_id == ""
    assert not h.client.connected
    assert vt.pending == []
    await vt.advance(3600)
    assert len(factory.sessions) == 1
    assert h.codes == [(CODE_FATAL, str(status))]


@pytest.mark.asyncio
async def test_init_5xx_keeps_the_fast_schedule(vt):
    factory = FakeFactory(
        init_error=lambda i: (
            SessionTokenError("unavailable", http_status=503) if i < 2 else None
        )
    )
    h = Harness(factory, vt)
    await h.client.start()
    await settle()

    assert h.codes == [(CODE_NON_FATAL, "503")]
    for delay in (1, 2):
        assert vt.pending == [delay]
        await vt.advance(delay)
    assert h.client.connected
    assert len(factory.sessions) == 3
    assert h.codes == [(CODE_NON_FATAL, "503")]


# -- mid-session events -------------------------------------------------------


@pytest.mark.asyncio
async def test_on_close_without_error_reconnects_fast(vt):
    h = await connected_harness(vt)
    first = h.factory.last

    first.fire_close()
    await settle()

    assert not h.client.connected
    assert h.disconnects == 1
    assert h.codes == [(CODE_NON_FATAL, "on_close")]
    assert first.closes == 1
    assert vt.pending == [1]

    await vt.advance(1)
    assert h.client.connected
    assert h.connected_ids == ["conn-0", "conn-1"]
    assert len(h.factory.sessions) == 2


@pytest.mark.asyncio
async def test_on_close_after_own_close_is_ignored(vt):
    """Review Focus 4: the SDK's on_close after our teardown is not a drop."""
    h = await connected_harness(vt)
    first = h.factory.last

    first.fire_error(sdk_error(None))
    await settle()
    # Teardown closed the session, which made the fake fire on_close once;
    # a stale duplicate from the SDK's read loop can still land afterwards.
    assert first.closes == 1
    first.kwargs["on_close"]()
    await settle()

    assert vt.pending == [1]
    await vt.advance(1)
    assert h.client.connected
    assert len(h.factory.sessions) == 2

    # Nothing else was scheduled: the new session stays up.
    first.kwargs["on_close"]()
    await vt.advance(60)
    assert h.client.connected
    assert len(h.factory.sessions) == 2
    assert h.codes == [(CODE_NON_FATAL, "AvatarSDKError")]
    assert h.disconnects == 1


@pytest.mark.asyncio
async def test_runtime_4010_server_error_only_reports_without_reconnect(vt):
    h = await connected_harness(vt)

    h.factory.last.fire_error(sdk_error(4010, server_code="4010"))
    await settle()

    assert h.client.connected
    assert h.codes == [(CODE_NON_FATAL, "4010")]
    assert h.reports[0].session_id == "conn-0"
    await vt.advance(60)
    assert len(h.factory.sessions) == 1
    assert h.client.connected


@pytest.mark.asyncio
async def test_runtime_4001_is_fatal(vt):
    h = await connected_harness(vt)
    first = h.factory.last

    first.fire_error(sdk_error(4001))
    await settle()

    assert not h.client.connected
    assert h.codes == [(CODE_FATAL, "4001")]
    assert first.closes == 1
    assert vt.pending == []
    await vt.advance(3600)
    assert len(h.factory.sessions) == 1


@pytest.mark.asyncio
async def test_send_audio_value_error_schedules_reconnect(vt):
    h = await connected_harness(vt)
    first = h.factory.last
    first.send_exc = ValueError("WebSocket connection is not established")

    assert not await h.client.send_audio(PCM)
    await settle()

    assert not h.client.connected
    assert h.codes == [(CODE_NON_FATAL, "ValueError")]
    assert first.closes == 1
    assert vt.pending == [1]
    await vt.advance(1)
    assert h.client.connected
    assert len(h.factory.sessions) == 2


@pytest.mark.asyncio
async def test_interrupt_value_error_is_not_a_drop(vt):
    h = await connected_harness(vt)
    h.factory.last.interrupt_exc = ValueError("interrupt: no request")

    assert not await h.client.interrupt()
    await settle()

    assert h.client.connected
    assert h.reports == []


@pytest.mark.asyncio
async def test_idle_timer_reconnects_only_when_no_request_open(vt):
    h = await connected_harness(vt, egress_idle_reconnect_ms=10_000)
    first = h.factory.last

    h.client.note_request_state(True)
    await vt.advance(12)
    assert h.client.connected
    assert len(h.factory.sessions) == 1

    h.client.note_request_state(False)
    await vt.advance(2)
    assert len(h.factory.sessions) == 2
    assert h.client.connected
    assert first.closes == 1
    # A planned reconnect is not an error.
    assert h.reports == []

    # Audio keeps the timer from firing.
    await vt.advance(6)
    assert await h.client.send_audio(PCM)
    await vt.advance(6)
    assert len(h.factory.sessions) == 2
    await vt.advance(6)
    assert len(h.factory.sessions) == 3


@pytest.mark.asyncio
async def test_transport_frames_triggers_reconnect(vt):
    h = await connected_harness(vt)
    first = h.factory.last

    first.fire_transport_frames()
    first.fire_transport_frames()
    await settle()

    assert not h.client.connected
    assert h.codes == [(CODE_NON_FATAL, "transport_frames")]
    assert first.closes == 1
    assert vt.pending == [1]
    await vt.advance(1)
    assert len(h.factory.sessions) == 2
    assert h.client.connected


@pytest.mark.asyncio
async def test_tokens_minted_per_connect(vt):
    h = await connected_harness(vt, agora_appcert="c")
    before = datetime.now(timezone.utc)

    h.factory.last.fire_close()
    await settle()
    await vt.advance(1)
    assert len(h.factory.sessions) == 2

    first, second = h.factory.sessions
    assert first.kwargs["agora_egress"] is not second.kwargs["agora_egress"]
    # A certificate signs a salted token: two mints never match.
    assert (
        first.kwargs["agora_egress"].token
        != second.kwargs["agora_egress"].token
    )
    for session in (first, second):
        expire_at = session.kwargs["expire_at"]
        assert expire_at.tzinfo is not None
        assert (
            timedelta(minutes=59)
            < expire_at - before
            < timedelta(hours=1, seconds=5)
        )


@pytest.mark.asyncio
async def test_stop_closes_and_silences(vt):
    h = await connected_harness(vt)
    first = h.factory.last

    await h.client.stop()
    await settle()

    assert not h.client.connected
    assert first.closes == 1
    assert h.reports == []
    assert not await h.client.send_audio(PCM)
    await vt.advance(3600)
    assert len(h.factory.sessions) == 1


@pytest.mark.asyncio
async def test_stop_during_backoff_cancels_the_retry(vt):
    factory = FakeFactory(start_error=lambda i: sdk_error(4503))
    h = Harness(factory, vt)
    await h.client.start()
    await settle()
    assert vt.pending == [1]

    await h.client.stop()
    await settle()
    assert vt.pending == []
    await vt.advance(3600)
    assert len(factory.sessions) == 1


@pytest.mark.asyncio
async def test_explicit_legacy_passes_false_and_zero_to_new_sdk(vt):
    h = await connected_harness(vt, source_timeline_only=False)
    egress = h.factory.last.kwargs["agora_egress"]
    assert egress.source_timeline_only is False
    assert egress.source_playback_profile == 0
    await h.client.stop()


@pytest.mark.asyncio
async def test_default_profile_requires_actual_server_confirmation(vt):
    factory = BurstFactory(lambda _: ())
    h = Harness(factory, vt)
    await h.client.start()
    await settle()
    assert not h.client.connected
    assert factory.last.closes == 1
    assert factory.last.sends == []
    assert h.codes == [(CODE_CONFIG, "UnsupportedConfigurationError")]
    assert vt.pending == []
    await h.client.stop()
