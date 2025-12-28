import threading
import pytest
from ten_runtime import App, TenEnv


class FakeApp(App):
    def __init__(self):
        super().__init__()
        self.event = threading.Event()

    def on_init(self, ten_env: TenEnv) -> None:
        self.event.set()
        ten_env.on_init_done()


@pytest.fixture(scope="session")
def app():
    """Create and return fake app for TEN runtime testing"""
    fake_app = FakeApp()
    return fake_app
