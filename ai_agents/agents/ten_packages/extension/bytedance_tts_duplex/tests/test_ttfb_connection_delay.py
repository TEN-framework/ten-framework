"""TTFB excludes waiting for the provider connection and session."""

import asyncio
import json
import threading
import time

import pytest
import websockets
from ten_runtime import Data, ExtensionTester, TenEnvTester, TenError, TenErrorCode

from .test_tts2_subtitles import (
    EVENT_ConnectionStarted,
    EVENT_Start_Connection,
    EVENT_StartSession,
    EVENT_SessionStarted,
    EVENT_TaskRequest,
    EVENT_TTSResponse,
    PCM_CHUNK,
    TTSSubtitleServer,
    _client_message,
    _server_response,
    _text_input,
)


class DelayedConnectionServer(TTSSubtitleServer):
    """Delay the connection acknowledgement before accepting queued text."""

    def __init__(self):
        super().__init__()
        self.text_received_at = None
        self.audio_sent_at = None

    async def _start_server(self):
        async def delay_handshake(connection, request):
            await asyncio.sleep(0.2)

        return await websockets.serve(
            self._handler,
            "127.0.0.1",
            0,
            process_request=delay_handshake,
        )

    async def _handler(self, websocket, *_args):
        async for raw in websocket:
            event, session_id, _ = _client_message(raw)
            if event == EVENT_Start_Connection:
                await websocket.send(_server_response(EVENT_ConnectionStarted))
            elif event == EVENT_StartSession:
                await websocket.send(
                    _server_response(EVENT_SessionStarted, session_id=session_id),
                )
            elif event == EVENT_TaskRequest:
                self.text_received_at = time.monotonic()
                await asyncio.sleep(0.02)
                self.audio_sent_at = time.monotonic()
                await websocket.send(
                    _server_response(
                        EVENT_TTSResponse,
                        session_id=session_id,
                        payload=PCM_CHUNK,
                    ),
                )


class TTFBTester(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb = None
        self.audio_received = False
        self.timeout = None

    def on_start(self, ten_env: TenEnvTester):
        self.timeout = threading.Timer(
            0.8,
            lambda: ten_env.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric,
                    "No first audio and TTFB received",
                )
            ),
        )
        self.timeout.start()
        ten_env.send_data(_text_input("delayed-connection", "Hello.", False, 1, 1))
        ten_env.on_start_done()

    def _complete(self, ten_env):
        if self.ttfb is not None and self.audio_received:
            self.timeout.cancel()
            ten_env.stop_test()

    def on_data(self, ten_env, data: Data):
        if data.get_name() == "metrics":
            raw, _ = data.get_property_to_json("")
            metrics = json.loads(raw)["metrics"]
            if "ttfb" in metrics:
                self.ttfb = metrics["ttfb"]
                self._complete(ten_env)

    def on_audio_frame(self, ten_env, frame):
        if frame.get_buf():
            self.audio_received = True
            self._complete(ten_env)


@pytest.mark.api
def test_ttfb_excludes_connection_wait():
    server = DelayedConnectionServer()
    server.start()
    try:
        tester = TTFBTester()
        tester.set_test_mode_single(
            "bytedance_tts_duplex",
            json.dumps(
                {
                    "app_id": "mock",
                    "token": "mock",
                    "url": server.uri,
                    "speaker": "mock",
                    "sample_rate": 16000,
                    "params": {},
                }
            ),
        )
        error = tester.run()
        assert error is None, error.error_message()
        assert server.text_received_at is not None
        assert server.audio_sent_at is not None
        # A 200 ms connection wait must not appear in the reported TTFB.
        provider_delay_ms = (server.audio_sent_at - server.text_received_at) * 1000
        assert 0 <= tester.ttfb < provider_delay_ms + 100
    finally:
        if tester.timeout is not None:
            tester.timeout.cancel()
        server.stop()
