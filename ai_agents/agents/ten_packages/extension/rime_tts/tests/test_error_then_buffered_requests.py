import asyncio
import json
from typing import Any
from unittest.mock import AsyncMock, patch

import pytest
from websockets.exceptions import ConnectionClosedOK

from ten_ai_base.message import (
    ModuleErrorVendorInfo,
    ModuleVendorException,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_runtime import AudioFrame, Data, ExtensionTester, TenEnvTester

from rime_tts.rime_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    RimeTTSClient,
    RimeTTSynthesizer,
)


class _TenEnvStub:
    @staticmethod
    def log_error(_message: str) -> None:
        pass


def test_server_error_remains_vendor_exception() -> None:
    synthesizer = RimeTTSynthesizer.__new__(RimeTTSynthesizer)
    synthesizer.ten_env = _TenEnvStub()
    synthesizer.vendor = "rime"

    with pytest.raises(ModuleVendorException):
        asyncio.run(
            synthesizer._handle_server_message(
                json.dumps({"type": "error", "message": "illegal input"})
            )
        )


class _ErrorThenBufferedRequestTester(ExtensionTester):
    def __init__(self) -> None:
        super().__init__()
        self.request4_started = False
        self.request4_end_reasons: list[int] = []
        self.current_audio_request_id: str | None = None
        self.request4_audio_bytes = 0

    def on_start(self, ten_env: TenEnvTester) -> None:
        for request_id, text in (
            ("request-3", "illegal input"),
            ("request-4", "must complete normally"),
        ):
            tts_input = TTSTextInput(
                request_id=request_id,
                text=text,
                text_input_end=True,
            )
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, tts_input.model_dump_json())
            ten_env.send_data(data)
        ten_env.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        payload_json, _ = data.get_property_to_json(None)
        payload: dict[str, Any] = (
            json.loads(payload_json) if payload_json else {}
        )

        if (
            data.get_name() == "tts_audio_start"
            and payload.get("request_id") == "request-4"
        ):
            self.request4_started = True
            self.current_audio_request_id = "request-4"
        elif (
            data.get_name() == "tts_audio_end"
            and payload.get("request_id") == "request-4"
        ):
            self.request4_end_reasons.append(payload.get("reason", 0))
            ten_env.stop_test()

    def on_audio_frame(
        self, _ten_env: TenEnvTester, audio_frame: AudioFrame
    ) -> None:
        buf = audio_frame.lock_buf()
        try:
            if self.current_audio_request_id == "request-4":
                self.request4_audio_bytes += len(buf)
        finally:
            audio_frame.unlock_buf(buf)


@patch("rime_tts.extension.RimeTTSClient")
def test_vendor_error_does_not_end_next_buffered_request(
    mock_client_class,
) -> None:
    mock_client: RimeTTSClient = mock_client_class.return_value
    mock_client.close = AsyncMock()
    mock_client.reset_synthesizer = AsyncMock()

    class _ErrorThenCloseWebSocket:
        def __init__(self) -> None:
            self.recv_count = 0

        async def recv(self) -> str:
            self.recv_count += 1
            if self.recv_count == 1:
                # Let request_tts record text_input_end before the vendor
                # reports that the final appended input is illegal.
                await asyncio.sleep(0.02)
                return "vendor error"
            if self.recv_count == 2:
                return "request-3 audio"
            await asyncio.sleep(0.1)
            raise ConnectionClosedOK(None, None)

    async def send_text(tts_input: TTSTextInput) -> None:
        if tts_input.request_id == "request-3":
            synthesizer = RimeTTSynthesizer.__new__(RimeTTSynthesizer)
            synthesizer.ten_env = mock_client.ten_env
            synthesizer.response_msgs = mock_client.response_msgs
            synthesizer._receive_ready_event = asyncio.Event()
            synthesizer._session_closing = False
            synthesizer.send_end_text = True
            synthesizer.latest_context_id = tts_input.request_id

            async def handle_server_message(message: str) -> None:
                if message == "vendor error":
                    raise ModuleVendorException(
                        ModuleErrorVendorInfo(
                            vendor="rime",
                            code="RIME_TTS_ERROR",
                            message="illegal input",
                        )
                    )
                await mock_client.response_msgs.put((EVENT_TTS_TTFB_METRIC, 10))
                await mock_client.response_msgs.put(
                    (EVENT_TTS_RESPONSE, b"\x09\x09")
                )

            synthesizer._handle_server_message = handle_server_message
            asyncio.create_task(
                synthesizer._receive_loop(_ErrorThenCloseWebSocket())
            )
            return

        async def emit_request4_audio() -> None:
            await asyncio.sleep(0.2)
            await mock_client.response_msgs.put((EVENT_TTS_TTFB_METRIC, 10))
            await mock_client.response_msgs.put(
                (EVENT_TTS_RESPONSE, b"\x01\x02\x03\x04")
            )
            await mock_client.response_msgs.put((EVENT_TTS_END, b""))

        asyncio.create_task(emit_request4_audio())

    mock_client.send_text = AsyncMock(side_effect=send_text)

    def create_client(config, ten_env, vendor, response_msgs, *_args):
        mock_client.ten_env = ten_env
        mock_client.response_msgs = response_msgs
        return mock_client

    mock_client_class.side_effect = create_client

    tester = _ErrorThenBufferedRequestTester()
    tester.set_test_mode_single(
        "rime_tts", json.dumps({"params": {"api_key": "test-key"}})
    )
    tester.run()

    assert tester.request4_started
    assert tester.request4_end_reasons == [TTSAudioEndReason.REQUEST_END]
    assert tester.request4_audio_bytes == 4
