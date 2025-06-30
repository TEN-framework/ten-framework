#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import threading
from types import SimpleNamespace
import pytest
from ten_runtime import (
    App,
    TenEnv,
)
from unittest.mock import AsyncMock, patch

class FakeApp(App):
    def __init__(self):
        super().__init__()
        self.event: threading.Event | None = None

    # In the case of a fake app, we use `on_init` to allow the blocked testing
    # fixture to continue execution, rather than using `on_configure`. The
    # reason is that in the TEN runtime C core, the relationship between the
    # addon manager and the (fake) app is bound after `on_configure_done` is
    # called. So we only need to let the testing fixture continue execution
    # after this action in the TEN runtime C core, and at the upper layer
    # timing, the earliest point is within the `on_init()` function of the upper
    # TEN app. Therefore, we release the testing fixture lock within the user
    # layer's `on_init()` of the TEN app.
    def on_init(self, ten_env: TenEnv) -> None:
        assert self.event
        self.event.set()

        ten_env.on_init_done()


class FakeAppCtx:
    def __init__(self, event: threading.Event):
        self.fake_app: FakeApp | None = None
        self.event = event


def run_fake_app(fake_app_ctx: FakeAppCtx):
    app = FakeApp()
    app.event = fake_app_ctx.event
    fake_app_ctx.fake_app = app
    app.run(False)



@pytest.fixture(scope="session", autouse=True)
def patch_deepgram_ws():
    """
    Automatically patch AsyncListenWebSocketClient globally before any test runs.
    """
    patch_target = "ten_packages.extension.deepgram_asr_python.extension.AsyncListenWebSocketClient"

    with patch(patch_target) as MockWSClient:
        print(f"✅ Patching {patch_target} before test session.")

        mock_ws = AsyncMock()
        mock_ws.start.return_value = True
        mock_ws.send.return_value = None
        mock_ws.finish.return_value = None

        mock_ws._handlers = {}

        def mock_on(event_name, callback):
            event_str = str(event_name) if not isinstance(event_name, str) else event_name
            mock_ws._handlers[event_str] = callback

        mock_ws.on = mock_on

        MockWSClient.return_value = mock_ws
        yield mock_ws
        # patch stays active through the whole session

@pytest.fixture(scope="session", autouse=True)
def global_setup_and_teardown():
    event = threading.Event()
    fake_app_ctx = FakeAppCtx(event)

    fake_app_thread = threading.Thread(
        target=run_fake_app, args=(fake_app_ctx,)
    )
    fake_app_thread.start()

    event.wait()

    assert fake_app_ctx.fake_app is not None

    # Yield control to the test; after the test execution is complete, continue
    # with the teardown process.
    yield

    # Teardown part.
    fake_app_ctx.fake_app.close()
    fake_app_thread.join()
