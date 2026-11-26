#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import importlib
import json
import threading
from types import SimpleNamespace

import pytest
from ten_runtime import App, TenEnv


class FakeApp(App):
    def __init__(self) -> None:
        super().__init__()
        self.ready: threading.Event | None = None

    def on_configure(self, ten_env: TenEnv) -> None:
        log = {
            "matchers": [{"level": "debug"}],
            "formatter": {"type": "plain", "colored": True},
            "emitter": {"type": "console", "config": {"stream": "stdout"}},
        }
        ten_env.init_property_from_json(
            json.dumps({"ten": {"log": {"handlers": [log]}}})
        )
        ten_env.on_configure_done()

    def on_init(self, ten_env: TenEnv) -> None:
        self.ready.set()
        ten_env.on_init_done()


@pytest.fixture(scope="session", autouse=True)
def fake_app():
    """The TEN app the extension testers run in, on a thread of its own."""
    apps: list[FakeApp] = []
    ready = threading.Event()

    def serve() -> None:
        app = FakeApp()
        app.ready = ready
        apps.append(app)
        app.run(False)

    thread = threading.Thread(target=serve)
    thread.start()
    ready.wait()
    yield
    apps[0].close()
    thread.join()


@pytest.fixture
def server():
    """A local Kalpa server for one test."""
    from mock_server import MockKalpaServer

    mock = MockKalpaServer().start()
    yield mock
    mock.stop()


@pytest.fixture
def config(server):
    """Extension properties that point at the local server."""
    from mock_server import API_KEY, SAMPLE_RATE

    params = {"api_key": API_KEY, "base_url": server.url}
    return {"params": {**params, "sample_rate": SAMPLE_RATE}}


@pytest.fixture
def running():
    """The extension's modules as the TEN runtime imports them.

    That is as the top-level package `kalpa_tts_python`, a second copy of
    what the tests import, so a test replaces things in this one.
    """
    return SimpleNamespace(
        extension=importlib.import_module("kalpa_tts_python.extension"),
        kalpa_tts=importlib.import_module("kalpa_tts_python.kalpa_tts"),
    )


class FakeClock:
    """Stands in for the `time` module and moves only when a test moves it.

    The steps used are multiples of 125 ms, which are exact in binary.
    """

    now = 1000.0

    def monotonic(self) -> float:
        return self.now

    def advance(self, ms: int) -> None:
        self.now += ms / 1000


@pytest.fixture
def clock(monkeypatch, running):
    fake = FakeClock()
    monkeypatch.setattr(running.extension, "time", fake)
    monkeypatch.setattr(running.kalpa_tts, "time", fake)
    return fake


@pytest.fixture
def blocked_send(monkeypatch, running):
    """The first text sent to Kalpa never leaves; set once it is stuck."""
    stuck = threading.Event()
    client = running.kalpa_tts.KalpaTTSClient
    send = client.send_text

    async def send_text(self, text, flush=False):
        if text.strip() and not stuck.is_set():
            stuck.set()
            await asyncio.sleep(3600)
        await send(self, text, flush)

    monkeypatch.setattr(client, "send_text", send_text)
    return stuck


@pytest.fixture
def slow_end(monkeypatch, running):
    """A request that ends with a disconnect takes a while over its last
    events; set once the first such request is ending."""
    ending = threading.Event()
    extension = running.extension.KalpaTTSExtension
    report = extension.on_disconnected

    async def on_disconnected(self, *args, **kwargs):
        if kwargs.get("message") != "stopped":
            ending.set()
            await asyncio.sleep(0.3)
        await report(self, *args, **kwargs)

    monkeypatch.setattr(extension, "on_disconnected", on_disconnected)
    return ending
