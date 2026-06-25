import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester
from websockets.protocol import State

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    GradiumTTSConnectionException,
    GradiumTTSClient,
)

MOCK_CONFIG = {
    "params": {
        "api_key": "test_api_key",
        "voice_id": "cLONiZ4hQ8VpQ4Sz",
        "sample_rate": 24000,
    },
}


def create_mock_client():
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.clean = AsyncMock()
    mock.cancel = AsyncMock()
    mock.get_ready_sample_rate.return_value = 24000
    mock.get_extra_metadata.return_value = {}
    fake_audio = b"\x00\x01\x02\x03" * 100

    async def mock_get(_text: str, *_args):
        yield 100, EVENT_TTS_TTFB_METRIC
        yield fake_audio, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock.get.side_effect = mock_get
    return mock


class SequentialRequestsTester(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.completed_request_ids = []
        self.audio_start_ids = []
        self.expected_ids = ["seq_req_1", "seq_req_2", "seq_req_3"]
        self.send_index = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        self._send_next(ten_env_tester)
        ten_env_tester.on_start_done()

    def _send_next(self, ten_env_tester: TenEnvTester) -> None:
        if self.send_index >= len(self.expected_ids):
            return
        request_id = self.expected_ids[self.send_index]
        tts_input = TTSTextInput(
            request_id=request_id,
            text=f"Hello from request {self.send_index + 1}.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        self.send_index += 1

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "tts_audio_start":
            json_str, _ = data.get_property_to_json("")
            self.audio_start_ids.append(json.loads(json_str).get("request_id"))
        elif name == "tts_audio_end":
            json_str, _ = data.get_property_to_json("")
            request_id = json.loads(json_str).get("request_id")
            self.completed_request_ids.append(request_id)
            if len(self.completed_request_ids) < len(self.expected_ids):
                self._send_next(ten_env)
            else:
                ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_sequential_requests(mock_client):
    mock_client.return_value = create_mock_client()
    tester = SequentialRequestsTester()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.completed_request_ids == [
        "seq_req_1",
        "seq_req_2",
        "seq_req_3",
    ]
    assert tester.audio_start_ids == [
        "seq_req_1",
        "seq_req_2",
        "seq_req_3",
    ]


class ReconnectAfterErrorTester(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.error_received = False
        self.second_audio_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="err_req_1",
            text="This will error.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            if not self.error_received:
                self.error_received = True
                tts_input = TTSTextInput(
                    request_id="ok_req_2",
                    text="This should work.",
                    text_input_end=True,
                )
                data2 = Data.create("tts_text_input")
                data2.set_property_from_json(None, tts_input.model_dump_json())
                ten_env.send_data(data2)
            else:
                self.second_audio_end = True
                ten_env.stop_test()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_reconnect_after_error(mock_client):
    call_count = 0

    def create_mock():
        mock = MagicMock()
        mock.start = AsyncMock()
        mock.clean = AsyncMock()
        mock.cancel = AsyncMock()
        mock.get_ready_sample_rate.return_value = 24000
        mock.get_extra_metadata.return_value = {}
        fake_audio = b"\x00\x01" * 200

        async def mock_get(_text: str, *_args):
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                yield b"Simulated error", EVENT_TTS_ERROR
            else:
                yield 100, EVENT_TTS_TTFB_METRIC
                yield fake_audio, EVENT_TTS_RESPONSE
                yield None, EVENT_TTS_END

        mock.get.side_effect = mock_get
        return mock

    mock_client.return_value = create_mock()
    tester = ReconnectAfterErrorTester()
    tester.set_test_mode_single("gradium_tts_python", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.second_audio_end


def test_config_redacts_api_key():
    config = GradiumTTSConfig(
        params={
            "api_key": "super-secret-key-12345",
            "voice_id": "cLONiZ4hQ8VpQ4Sz",
        }
    )
    config.update_params()
    safe_str = config.to_str(sensitive_handling=True)

    assert "super-secret-key-12345" not in safe_str
    assert "cLONiZ4hQ8VpQ4Sz" in safe_str


def test_client_empty_text_yields_end():
    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )
        client = GradiumTTSClient(config=config, ten_env=ten_env)

        events = []
        async for _data, event in client.get("", "req", True):
            events.append(event)

        assert events == [EVENT_TTS_END]
        assert client.ws is None

    asyncio.run(_run())


def test_client_reuses_open_connection_without_closed_attr():
    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        ten_env.log_debug = MagicMock()
        config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )
        client = GradiumTTSClient(config=config, ten_env=ten_env)

        ws = MagicMock()
        ws.state = State.OPEN
        client.ws = ws

        with patch(
            "gradium_tts_python.gradium_tts.websockets.connect"
        ) as connect:
            await client._connect()
            connect.assert_not_called()

    asyncio.run(_run())


def test_client_clean_close_is_treated_as_end():
    class CleanClose(Exception):
        def __init__(self):
            super().__init__("sent 1000 (OK); then received 1000 (OK)")
            self.code = 1000

    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        ten_env.log_debug = MagicMock()
        config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )
        client = GradiumTTSClient(config=config, ten_env=ten_env)

        ws = MagicMock()
        ws.recv = AsyncMock(side_effect=CleanClose())
        client.ws = ws

        events = []
        async for data, event in client._iter_messages():
            events.append((data, event))

        assert events == [(None, EVENT_TTS_END)]

    asyncio.run(_run())


def test_clean_close_before_ready_is_connection_error():
    class CleanClose(Exception):
        def __init__(self):
            super().__init__("sent 1000 (OK); then received 1000 (OK)")
            self.code = 1000

    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        ten_env.log_debug = MagicMock()
        config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )
        client = GradiumTTSClient(config=config, ten_env=ten_env)

        ws = MagicMock()
        ws.recv = AsyncMock(side_effect=CleanClose())
        client.ws = ws

        try:
            await client._wait_for_ready()
        except Exception as exc:
            assert "connection failed" in str(exc).lower()
        else:
            assert False, "Expected _wait_for_ready to raise"

    asyncio.run(_run())


def test_auth_error_message_code_1008_maps_to_fatal_connection_error():
    ten_env = MagicMock()
    config = GradiumTTSConfig(
        api_key="test",
        voice_id="cLONiZ4hQ8VpQ4Sz",
        sample_rate=24000,
    )
    client = GradiumTTSClient(config=config, ten_env=ten_env)

    exc = client._message_to_exception(
        {
            "type": "error",
            "code": 1008,
            "message": "Invalid or expired API key",
        }
    )

    assert isinstance(exc, GradiumTTSConnectionException)
    assert exc.status_code == 401


def test_client_get_forces_fresh_connection_per_request():
    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        ten_env.log_debug = MagicMock()
        ten_env.log_error = MagicMock()
        config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )
        client = GradiumTTSClient(config=config, ten_env=ten_env)

        client._disconnect = AsyncMock()
        client._connect = AsyncMock()
        client._send_setup = AsyncMock()
        client._wait_for_ready = AsyncMock()
        client._send_json = AsyncMock()

        async def fake_iter_messages():
            yield None, EVENT_TTS_END

        client._iter_messages = fake_iter_messages

        events = []
        async for _data, event in client.get("hello", "req-1", True):
            events.append(event)

        client._disconnect.assert_any_await()
        client._connect.assert_awaited_once()
        assert events == [EVENT_TTS_END]

    asyncio.run(_run())
