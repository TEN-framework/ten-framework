import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester
from websockets.protocol import State

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.extension import GradiumTTSExtension
from gradium_tts_python.gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_ERROR,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    GradiumTTSConnectionException,
    GradiumTTSClient,
)

from .gradium_mocks import make_streaming_mock_client

MOCK_CONFIG = {
    "params": {
        "api_key": "test_api_key",
        "voice_id": "cLONiZ4hQ8VpQ4Sz",
        "sample_rate": 24000,
    },
}


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
    mock_client.return_value = make_streaming_mock_client()
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
    """First session errors mid-stream; the next request opens a fresh
    session and succeeds."""
    call_count = {"n": 0}
    state = {"queue": None}

    mock = MagicMock()
    mock.start = AsyncMock()
    mock.clean = AsyncMock()
    mock.cancel = AsyncMock()
    mock.get_ready_sample_rate.return_value = 24000
    mock.get_extra_metadata.return_value = {}

    async def _start_session():
        call_count["n"] += 1
        queue: asyncio.Queue = asyncio.Queue()
        if call_count["n"] == 1:
            queue.put_nowait((b"Simulated error", EVENT_TTS_ERROR))
        else:
            queue.put_nowait((100, EVENT_TTS_TTFB_METRIC))
            queue.put_nowait((b"\x00\x01" * 200, EVENT_TTS_RESPONSE))
        state["queue"] = queue

    mock.start_session = AsyncMock(side_effect=_start_session)
    mock.send_text = AsyncMock()

    async def _end_input():
        if state["queue"] is not None:
            state["queue"].put_nowait((None, EVENT_TTS_END))

    mock.end_input = AsyncMock(side_effect=_end_input)

    def _audio_events():
        queue = state["queue"]

        async def _gen():
            while True:
                item = await queue.get()
                yield item
                if item[1] in (EVENT_TTS_END, EVENT_TTS_ERROR):
                    return

        return _gen()

    mock.audio_events.side_effect = _audio_events
    mock_client.return_value = mock

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


def test_client_streaming_session_round_trip():
    """start_session connects + sets up; send_text/end_input drive the wire
    protocol; audio_events streams audio then ends."""

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

        client._disconnect = AsyncMock()
        client._connect = AsyncMock()
        client._send_setup = AsyncMock()
        client._wait_for_ready = AsyncMock()
        sent: list = []

        async def _send_json(payload):
            sent.append(payload)

        client._send_json = AsyncMock(side_effect=_send_json)

        await client.start_session()
        client._disconnect.assert_any_await()
        client._connect.assert_awaited_once()
        client._send_setup.assert_awaited_once()
        client._wait_for_ready.assert_awaited_once()

        await client.send_text("hello")
        await client.send_text(" world")
        await client.end_input()

        assert {"type": "text", "text": "hello"} in sent
        assert {"type": "text", "text": " world"} in sent
        assert {"type": "end_of_stream"} in sent

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


def test_websockets14_clean_close_is_treated_as_end():
    """Regression: on websockets >= 14 the close code lives on
    exc.rcvd.code / exc.sent.code, not a top-level exc.code. A clean 1000
    close in that shape must still map to EVENT_TTS_END, not EVENT_TTS_ERROR.
    """

    class CloseFrame:
        def __init__(self, code: int):
            self.code = code

    class ConnectionClosedOKLike(Exception):
        # Mirrors websockets>=14: no top-level .code, frame on .rcvd/.sent.
        def __init__(self):
            super().__init__("received 1000 (OK); then sent 1000 (OK)")
            self.rcvd = CloseFrame(1000)
            self.sent = CloseFrame(1000)

    assert not hasattr(ConnectionClosedOKLike(), "code")

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
        ws.recv = AsyncMock(side_effect=ConnectionClosedOKLike())
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


def test_client_start_session_forces_fresh_connection():
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

        await client.start_session()

        client._disconnect.assert_any_await()
        client._connect.assert_awaited_once()

    asyncio.run(_run())


def test_send_text_failure_cancels_reader_task():
    """If send_text fails after the session opened, the request finalizes with
    an error and the background reader task is cancelled, not left running."""

    async def _run():
        ext = GradiumTTSExtension("gradium_tts_python")
        ext.ten_env = MagicMock()
        ext.config = GradiumTTSConfig(
            api_key="test",
            voice_id="cLONiZ4hQ8VpQ4Sz",
            sample_rate=24000,
        )

        # Base-class output hooks invoked during finalize.
        ext.send_tts_audio_start = AsyncMock()
        ext.send_tts_audio_end = AsyncMock()
        ext.send_tts_audio_data = AsyncMock()
        ext.finish_request = AsyncMock()
        ext.metrics_add_output_characters = MagicMock()
        ext.metrics_add_recv_audio_chunks = MagicMock()

        client = MagicMock()
        client.start_session = AsyncMock()
        client.get_ready_sample_rate.return_value = 24000
        client.get_extra_metadata.return_value = {}
        client.send_text = AsyncMock(side_effect=RuntimeError("ws broke"))

        async def _audio_events():
            # Blocks until the reader task is cancelled.
            await asyncio.Event().wait()
            yield None, EVENT_TTS_END

        client.audio_events.side_effect = lambda: _audio_events()
        ext.client = client

        await ext.request_tts(
            TTSTextInput(request_id="r1", text="hello", text_input_end=True)
        )

        assert ext._reader_task is None
        ext.send_tts_audio_end.assert_awaited()
        ext.finish_request.assert_awaited()

    asyncio.run(_run())
