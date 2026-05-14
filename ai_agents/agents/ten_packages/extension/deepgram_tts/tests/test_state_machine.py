#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from unittest.mock import patch, AsyncMock, MagicMock


from ten_runtime import (
    ExtensionTester,
    TenEnvTester,
    Data,
)
from ten_ai_base.struct import TTSTextInput
from deepgram_tts.deepgram_tts import (
    EVENT_TTS_RESPONSE,
    EVENT_TTS_END,
    EVENT_TTS_TTFB_METRIC,
    EVENT_TTS_ERROR,
    DeepgramTTSClient,
)
from deepgram_tts.config import DeepgramTTSConfig

MOCK_CONFIG = {
    "params": {
        "api_key": "test_api_key",
        "model": "aura-2-thalia-en",
        "encoding": "linear16",
        "sample_rate": 24000,
    },
}


def create_mock_client():
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.stop = AsyncMock()
    mock.cancel = AsyncMock()
    mock.reset_ttfb = lambda: None
    fake_audio = b"\x00\x01\x02\x03" * 100

    async def mock_get(text):
        yield (100, EVENT_TTS_TTFB_METRIC)
        yield (fake_audio, EVENT_TTS_RESPONSE)
        yield (None, EVENT_TTS_END)

    mock.get.side_effect = mock_get
    return mock


# ================ test sequential requests ================
class SequentialRequestsTester(ExtensionTester):
    """Send 3 requests with different IDs sequentially.

    Each request should produce tts_audio_start, audio
    frames, and tts_audio_end with the correct request_id.
    """

    def __init__(self):
        super().__init__()
        self.completed_request_ids = []
        self.audio_start_ids = []
        self.expected_ids = [
            "seq_req_1",
            "seq_req_2",
            "seq_req_3",
        ]
        self.send_index = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        ten_env_tester.log_info("Sequential requests test started.")
        self._send_next(ten_env_tester)
        ten_env_tester.on_start_done()

    def _send_next(self, ten_env_tester: TenEnvTester) -> None:
        if self.send_index >= len(self.expected_ids):
            return
        req_id = self.expected_ids[self.send_index]
        tts_input = TTSTextInput(
            request_id=req_id,
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
            d = json.loads(json_str) if json_str else {}
            rid = d.get("request_id", "")
            self.audio_start_ids.append(rid)
        elif name == "tts_audio_end":
            json_str, _ = data.get_property_to_json("")
            d = json.loads(json_str) if json_str else {}
            rid = d.get("request_id", "")
            self.completed_request_ids.append(rid)
            ten_env.log_info(f"Completed request: {rid}")
            if len(self.completed_request_ids) < len(self.expected_ids):
                self._send_next(ten_env)
            else:
                ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_sequential_requests(MockClient):
    """Each sequential request should complete with its own
    request_id in audio_start and audio_end."""
    MockClient.return_value = create_mock_client()

    tester = SequentialRequestsTester()
    tester.set_test_mode_single("deepgram_tts", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.completed_request_ids == [
        "seq_req_1",
        "seq_req_2",
        "seq_req_3",
    ], (
        f"Expected 3 sequential completions, got "
        f"{tester.completed_request_ids}"
    )
    assert tester.audio_start_ids == [
        "seq_req_1",
        "seq_req_2",
        "seq_req_3",
    ], f"audio_start ids mismatch: {tester.audio_start_ids}"


# ================ test reconnect after error ================
class ReconnectAfterErrorTester(ExtensionTester):
    """First request errors, second request should succeed.

    Validates that the client recovers after a mid-stream
    failure.
    """

    def __init__(self):
        super().__init__()
        self.error_received = False
        self.second_audio_end = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        # First request will trigger an error
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
        name = data.get_name()
        if name == "tts_audio_end":
            if not self.error_received:
                # First request ended (with error) — send
                # second request
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


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_reconnect_after_error(MockClient):
    """After an error, subsequent requests should succeed."""
    call_count = 0

    def create_mock():
        mock = MagicMock()
        mock.start = AsyncMock()
        mock.stop = AsyncMock()
        mock.cancel = AsyncMock()
        mock.reset_ttfb = lambda: None

        fake_audio = b"\x00\x01" * 200

        async def mock_get(text):
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                # First call: error
                yield (
                    b"Simulated error",
                    EVENT_TTS_ERROR,
                )
            else:
                # Subsequent calls: success
                yield (100, EVENT_TTS_TTFB_METRIC)
                yield (fake_audio, EVENT_TTS_RESPONSE)
                yield (None, EVENT_TTS_END)

        mock.get.side_effect = mock_get
        return mock

    MockClient.return_value = create_mock()

    tester = ReconnectAfterErrorTester()
    tester.set_test_mode_single("deepgram_tts", json.dumps(MOCK_CONFIG))
    tester.run()

    assert (
        tester.second_audio_end
    ), "Second request should complete after first errored."


# ================ test config redaction ================
def test_config_redacts_api_key():
    """to_str(sensitive_handling=True) must not leak the
    API key."""
    config = DeepgramTTSConfig(
        params={
            "api_key": "super-secret-key-12345",
            "model": "aura-2-thalia-en",
        }
    )
    config.update_params()

    safe_str = config.to_str(sensitive_handling=True)

    assert "super-secret-key-12345" not in safe_str
    assert "aura-2-thalia-en" in safe_str


# ================ test empty text yields END ================
def test_client_empty_text_yields_end():
    """get() with empty text should yield EVENT_TTS_END
    immediately without connecting."""

    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        config = DeepgramTTSConfig(api_key="test")
        client = DeepgramTTSClient(config=config, ten_env=ten_env)

        events = []
        async for data, event in client.get(""):
            events.append(event)

        assert events == [EVENT_TTS_END]
        assert client._ws is None  # no connection made

    asyncio.run(_run())


def test_client_whitespace_text_yields_end():
    """get() with whitespace-only text should yield
    EVENT_TTS_END."""

    async def _run():
        ten_env = MagicMock()
        ten_env.log_warn = MagicMock()
        config = DeepgramTTSConfig(api_key="test")
        client = DeepgramTTSClient(config=config, ten_env=ten_env)

        events = []
        async for data, event in client.get("   \n\t  "):
            events.append(event)

        assert events == [EVENT_TTS_END]

    asyncio.run(_run())


# ================ test 401 emits exactly one error ================
class AuthErrorTester(ExtensionTester):
    """Validates that a 401 auth failure emits exactly one
    error event and one terminal audio_end."""

    def __init__(self):
        super().__init__()
        self.error_count = 0
        self.audio_end_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="auth_err_req",
            text="This should fail with 401.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "error":
            self.error_count += 1
        elif name == "tts_audio_end":
            self.audio_end_count += 1
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_auth_error_single_emission(MockClient):
    """401 should produce exactly 1 error event, not
    duplicates."""
    from deepgram_tts.deepgram_tts import (
        DeepgramTTSConnectionException,
    )

    mock = MagicMock()
    mock.start = AsyncMock()
    mock.stop = AsyncMock()
    mock.cancel = AsyncMock()
    mock.reset_ttfb = lambda: None

    async def mock_get_auth_fail(text):
        raise DeepgramTTSConnectionException(
            status_code=401, body="Unauthorized"
        )
        yield  # make it a generator  # pragma: no cover

    mock.get.side_effect = mock_get_auth_fail
    MockClient.return_value = mock

    tester = AuthErrorTester()
    tester.set_test_mode_single("deepgram_tts", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.error_count == 1, (
        f"Expected exactly 1 error event, got " f"{tester.error_count}"
    )


# ================ test non-final error contract ================
class NonFinalErrorTester(ExtensionTester):
    """Validates that an error on a non-final chunk does NOT
    produce a public error event. Partial stream errors are
    transient — only logged, not surfaced to callers."""

    def __init__(self):
        super().__init__()
        self.error_count = 0
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        # First chunk: non-final, will error
        tts_input = TTSTextInput(
            request_id="nonfinal_req",
            text="First chunk errors.",
            text_input_end=False,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)

        # Second chunk: final, succeeds
        tts_input2 = TTSTextInput(
            request_id="nonfinal_req",
            text="Second chunk works.",
            text_input_end=True,
        )
        data2 = Data.create("tts_text_input")
        data2.set_property_from_json(None, tts_input2.model_dump_json())
        ten_env_tester.send_data(data2)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "error":
            self.error_count += 1
        elif name == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()


@patch("deepgram_tts.extension.DeepgramTTSClient")
def test_nonfinal_error_not_surfaced(MockClient):
    """Error on non-final chunk should not emit public
    error event. This is the intended contract: partial
    stream errors are transient."""
    call_count = 0

    def create_mock():
        mock = MagicMock()
        mock.start = AsyncMock()
        mock.stop = AsyncMock()
        mock.cancel = AsyncMock()
        mock.reset_ttfb = lambda: None

        fake_audio = b"\x00\x01" * 200

        async def mock_get(text):
            nonlocal call_count
            call_count += 1
            if call_count == 1:
                yield (b"Transient error", EVENT_TTS_ERROR)
            else:
                yield (100, EVENT_TTS_TTFB_METRIC)
                yield (fake_audio, EVENT_TTS_RESPONSE)
                yield (None, EVENT_TTS_END)

        mock.get.side_effect = mock_get
        return mock

    MockClient.return_value = create_mock()

    tester = NonFinalErrorTester()
    tester.set_test_mode_single("deepgram_tts", json.dumps(MOCK_CONFIG))
    tester.run()

    assert tester.error_count == 0, (
        f"Non-final error should not produce public error "
        f"event, got {tester.error_count}"
    )
    assert (
        tester.audio_end_received
    ), "Request should still complete after non-final error"
