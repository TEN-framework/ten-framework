import asyncio
import threading
import time
from unittest.mock import AsyncMock, MagicMock, patch

from ..tencent_tts import TencentTTSClient
from ..config import TencentTTSConfig


def _make_config(**overrides) -> TencentTTSConfig:
    defaults = dict(
        app_id="123456",
        secret_id="fake_id",
        secret_key="fake_key",
        voice_type=0,
    )
    defaults.update(overrides)
    return TencentTTSConfig(**defaults)


def _make_mock_ten_env() -> MagicMock:
    env = MagicMock()
    env.log_debug = MagicMock()
    env.log_info = MagicMock()
    env.log_error = MagicMock()
    env.log_warn = MagicMock()
    return env


class FakeSynthesizerFail:
    """Simulates a synthesizer whose server rejects the request
    (e.g. invalid voice_type)."""

    def __init__(self, callback):
        self.callback = callback
        self.ready = False
        self.ready_event = threading.Event()

    def set_voice_type(self, v):
        pass

    def set_codec(self, v):
        pass

    def set_sample_rate(self, v):
        pass

    def set_enable_subtitle(self, v):
        pass

    def set_speed(self, v):
        pass

    def set_volume(self, v):
        pass

    def set_emotion_category(self, v):
        pass

    def set_emotion_intensity(self, v):
        pass

    def start(self):
        def _bg():
            time.sleep(0.05)
            self.callback.on_synthesis_fail(
                {
                    "code": 12345,
                    "message": "invalid voice type",
                    "request_id": "r1",
                }
            )
            if not self.ready:
                self.ready_event.set()

        threading.Thread(target=_bg, daemon=True).start()

    def wait_ready(self, timeout_ms):
        return self.ready_event.wait(timeout_ms / 1000.0)

    def is_alive(self):
        return False


def test_start_does_not_block_event_loop_on_failure():
    """Verify start() does not block the asyncio event loop.

    Run start() and a probe coroutine via gather(). The probe just
    does ``await asyncio.sleep(0)`` (a single yield). If the loop is
    free the probe completes; if start() blocks the loop thread the
    probe never gets scheduled.
    """

    async def _run():
        config = _make_config(voice_type=999999)
        ten_env = _make_mock_ten_env()
        client = TencentTTSClient(config, ten_env, "tencent")

        probe_ran = False

        async def probe():
            nonlocal probe_ran
            await asyncio.sleep(0)
            probe_ran = True

        with patch(
            "tencent_tts_python.tencent_tts.FlowingSpeechSynthesizer",
            side_effect=lambda appid, cred, cb: FakeSynthesizerFail(cb),
        ):
            t0 = time.monotonic()

            start_exc = None
            try:
                await asyncio.gather(client.start(), probe())
            except Exception as e:
                start_exc = e

            elapsed = time.monotonic() - t0

        # start() should have raised (synthesizer.ready is False)
        assert start_exc is not None, "start() should have raised on failure"

        # probe must have run, proving the loop was not blocked
        assert (
            probe_ran
        ), "Probe coroutine never ran - event loop was blocked by start()"

        # Should complete quickly, not wait for the 5s timeout
        assert elapsed < 2.0, f"start() took {elapsed:.2f}s, expected < 2s"

    asyncio.run(_run())


def test_stop_does_not_restart_on_auth_error():
    """stop() should NOT auto-restart when there was an auth error."""

    async def _run():
        config = _make_config()
        ten_env = _make_mock_ten_env()
        client = TencentTTSClient(config, ten_env, "tencent")

        fake_callback = MagicMock()
        fake_callback.auth_error = True
        client._callback = fake_callback
        client.synthesizer = MagicMock()

        client.start = AsyncMock()
        await client.stop()
        client.start.assert_not_called()

    asyncio.run(_run())
