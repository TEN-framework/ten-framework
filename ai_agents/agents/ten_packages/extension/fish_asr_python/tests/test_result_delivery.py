"""Exercise Fish results through TEN's native message schema validation."""

import asyncio
import json
import os
import shutil
import threading
from contextlib import contextmanager
from pathlib import Path

import ormsgpack
import pytest
from ten_runtime import (
    App,
    AsyncExtensionTester,
    AudioFrame,
    Data,
    register_addon_as_extension,
)
from websockets.sync.server import serve

from ..addon import FishAudioASRExtensionAddon

TEST_ADDON = "fish_asr_result_delivery_test"


@pytest.fixture(scope="module")
def runtime_app(tmp_path_factory):
    ready = threading.Event()
    apps = []
    # Standalone installs link the addon to its source directory, where the
    # manifest's ../../system import does not resolve. Give the real Fish
    # factory a test name and a physical package layout for schema validation.
    app_dir = Path(os.environ["TEN_APP_BASE_DIR"]).absolute()
    package_root = tmp_path_factory.mktemp("fish-delivery") / "ten_packages"
    addon_dir = package_root / "extension" / TEST_ADDON
    addon_dir.mkdir(parents=True)
    manifest = json.loads(
        (Path(__file__).parents[1] / "manifest.json").read_text()
    )
    manifest["name"] = TEST_ADDON
    (addon_dir / "manifest.json").write_text(json.dumps(manifest))
    schema_dir = package_root / "system/ten_ai_base/api"
    schema_dir.mkdir(parents=True)
    shutil.copyfile(
        app_dir / "ten_packages/system/ten_ai_base/api/asr-interface.json",
        schema_dir / "asr-interface.json",
    )

    register_addon_as_extension(
        TEST_ADDON, base_dir=str(addon_dir / "addon.py")
    )(FishAudioASRExtensionAddon)

    class TestApp(App):
        def on_init(self, ten_env):
            ready.set()
            ten_env.on_init_done()

    def run_app():
        app = TestApp()
        apps.append(app)
        app.run(False)

    thread = threading.Thread(target=run_app)
    thread.start()
    try:
        assert ready.wait(10), "TEN test app did not initialize"
        yield
    finally:
        apps[0].close()
        thread.join(timeout=10)
        assert not thread.is_alive(), "TEN test app did not stop"


@contextmanager
def fish_server(word_fields):
    audio_received = threading.Event()
    requests = []

    def handle_socket(socket):
        for frame in socket:
            message = ormsgpack.unpackb(frame)
            requests.append(message["type"])
            if message["type"] == "config":
                socket.send(ormsgpack.packb({"type": "session_started"}))
            elif message["type"] == "audio":
                for event in [
                    {"type": "speech_start", "audio_ms": 100},
                    {"type": "user_turn_taken"},
                    {"type": "draft", "text": "hello"},
                    {"type": "eager_end_of_turn", "text": "hello there"},
                ]:
                    socket.send(
                        ormsgpack.packb(
                            {
                                "turn_index": 0,
                                "audio_ms": 500,
                                "language_code": "en",
                                **event,
                            }
                        )
                    )
                audio_received.set()
            elif message["type"] == "finalize":
                socket.send(
                    ormsgpack.packb(
                        {
                            "type": "committed",
                            "turn_index": 0,
                            "text": "hello there",
                            "audio_start_ms": 100,
                            "audio_end_ms": 500,
                            "language_code": "en",
                            "user_turn": True,
                            "reason": "finalize",
                            **word_fields,
                        }
                    )
                )
            elif message["type"] == "audio_done":
                socket.send(ormsgpack.packb({"type": "session_done"}))

    with serve(handle_socket, "127.0.0.1", 0) as server:
        thread = threading.Thread(target=server.serve_forever)
        thread.start()
        try:
            port = server.socket.getsockname()[1]
            yield f"ws://127.0.0.1:{port}", audio_received, requests
        finally:
            server.shutdown()
            thread.join(timeout=10)
            assert not thread.is_alive(), "Fish test server did not stop"


class ResultDeliveryTester(AsyncExtensionTester):
    def __init__(self, audio_received):
        super().__init__()
        self.audio_received = audio_received
        self.received = []
        self.timed_out = False
        self.watchdog = None

    async def on_start(self, ten_env):
        async def timeout():
            await asyncio.sleep(10)
            self.timed_out = True
            ten_env.stop_test()

        self.watchdog = asyncio.create_task(timeout())
        frame = AudioFrame.create("pcm_frame")
        frame.set_sample_rate(16000)
        frame.set_number_of_channels(1)
        frame.set_bytes_per_sample(2)
        frame.set_samples_per_channel(8000)
        frame.set_property_from_json(
            "metadata", json.dumps({"session_id": "delivery-session"})
        )
        frame.alloc_buf(16000)
        buffer = frame.lock_buf()
        buffer[:] = b"\0" * 16000
        frame.unlock_buf(buffer)
        assert await ten_env.send_audio_frame(frame) is None
        assert await asyncio.to_thread(self.audio_received.wait, 5)
        finalize = Data.create("asr_finalize")
        finalize.set_property_from_json(
            None,
            json.dumps(
                {
                    "finalize_id": "delivery-finalize",
                    "metadata": {"session_id": "delivery-session"},
                }
            ),
        )
        assert await ten_env.send_data(finalize) is None

    async def on_data(self, ten_env, data):
        name = data.get_name()
        if name not in {"asr_result", "asr_finalize_end", "error"}:
            return
        payload, error = data.get_property_to_json("")
        assert error is None
        self.received.append((name, json.loads(payload)))
        if name == "asr_finalize_end":
            ten_env.stop_test()

    async def on_stop(self, _ten_env):
        if self.watchdog is not None:
            self.watchdog.cancel()
            await asyncio.gather(self.watchdog, return_exceptions=True)


@pytest.mark.parametrize(
    "word_fields, expected_words",
    [
        pytest.param({}, [], id="missing-words"),
        pytest.param({"words": []}, [], id="empty-words"),
        pytest.param(
            {
                "words": [
                    {"text": "hello", "start_ms": 100, "end_ms": 300},
                    {"text": "there", "start_ms": 300, "end_ms": 500},
                ]
            },
            [
                {
                    "word": "hello",
                    "start_ms": 100,
                    "duration_ms": 200,
                    "stable": True,
                },
                {
                    "word": "there",
                    "start_ms": 300,
                    "duration_ms": 200,
                    "stable": True,
                },
            ],
            id="timed-words",
        ),
    ],
)
@pytest.mark.usefixtures("runtime_app")
def test_results_reach_consumer_before_finalize_end(
    word_fields, expected_words
):
    with fish_server(word_fields) as (url, audio_received, requests):
        tester = ResultDeliveryTester(audio_received)
        tester.set_test_mode_single(
            TEST_ADDON,
            json.dumps({"params": {"api_key": "test-key", "url": url}}),
        )
        error = tester.run()

    assert error is None
    assert not tester.timed_out, tester.received
    assert requests == ["config", "audio", "finalize", "audio_done"]
    assert [name for name, _ in tester.received] == [
        "asr_result",
        "asr_result",
        "asr_result",
        "asr_finalize_end",
    ]
    draft, eager, final, finalize_end = [
        payload for _, payload in tester.received
    ]
    assert [result["text"] for result in (draft, eager, final)] == [
        "hello",
        "hello there",
        "hello there",
    ]
    assert [result["final"] for result in (draft, eager, final)] == [
        False,
        False,
        True,
    ]
    assert draft["words"] == eager["words"] == []
    assert final["words"] == expected_words
    assert eager["metadata"]["asr_info"]["eager_end_of_turn"] is True
    assert draft["id"] == eager["id"] == final["id"]
    assert final["id"]
    for result in (draft, eager, final):
        assert result["metadata"]["session_id"] == "delivery-session"
        assert result["metadata"]["asr_info"]["vendor"] == "fish_audio"
        assert result["language"] == "en-US"
        assert result["start_ms"] == 100
        assert result["duration_ms"] == 400
    assert finalize_end == {
        "finalize_id": "delivery-finalize",
        "metadata": {"session_id": "delivery-session"},
    }
