import json
import threading

import pytest
from ten_runtime import App, TenEnv


class FakeApp(App):
    def __init__(self):
        super().__init__()
        self.ready = threading.Event()

    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.init_property_from_json(json.dumps({"ten": {"log_level": 2}}))
        ten_env.on_configure_done()

    def on_init(self, ten_env: TenEnv) -> None:
        self.ready.set()
        ten_env.on_init_done()


@pytest.fixture(scope="session", autouse=True)
def ten_app():
    ready = threading.Event()
    apps = []

    def run():
        app = FakeApp()
        app.ready = ready
        apps.append(app)
        app.run(False)

    thread = threading.Thread(target=run)
    thread.start()
    assert ready.wait(15), "TEN app initialization timed out"
    yield
    apps[0].close()
    thread.join(timeout=15)
    assert not thread.is_alive(), "TEN app did not stop"
