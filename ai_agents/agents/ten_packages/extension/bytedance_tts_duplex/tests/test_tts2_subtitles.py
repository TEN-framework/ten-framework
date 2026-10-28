"""Duplex error recovery and TTS 1.0/2.0 subtitles through TEN and vendor boundaries."""

from __future__ import annotations

import asyncio
import collections
import json
import threading
from typing import Any

import websockets
from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.struct import TTSTextResult
from ten_runtime import (
    AudioFrame,
    Cmd,
    CmdResult,
    Data,
    ExtensionTester,
    StatusCode,
    TenEnvTester,
    TenError,
    TenErrorCode,
)

from ..bytedance_tts import (
    ERROR_INFORMATION,
    EVENT_ConnectionFailed,
    EVENT_ConnectionStarted,
    EVENT_FinishConnection,
    EVENT_FinishSession,
    EVENT_SessionFinished,
    EVENT_SessionStarted,
    EVENT_Start_Connection,
    EVENT_StartSession,
    EVENT_TaskRequest,
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
    EVENT_TTSSentenceStart,
    FULL_SERVER_RESPONSE,
    Header,
    JSON,
    MsgTypeFlagWithEvent,
)

EVENT_TTS_SUBTITLE = 364
PCM_CHUNK = b"\x01\x00" * 12_800


def _content(value: str) -> bytes:
    encoded = value.encode("utf-8")
    return len(encoded).to_bytes(4, "big") + encoded


def _server_response(
    event: int,
    *,
    session_id: str | None = None,
    payload: bytes | None = None,
    payload_json: str | None = None,
) -> bytes:
    response = bytearray(
        Header(
            message_type=FULL_SERVER_RESPONSE,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes(),
    )
    response.extend(event.to_bytes(4, "big"))

    if event == EVENT_ConnectionStarted:
        response.extend(_content("mock-connection"))
    elif event == EVENT_ConnectionFailed:
        response.extend(_content("{}"))
    elif event in (EVENT_SessionStarted, EVENT_SessionFinished):
        assert session_id is not None
        response.extend(_content(session_id))
        response.extend(_content("{}"))
    elif event == EVENT_TTSResponse:
        assert session_id is not None
        assert payload is not None
        response.extend(_content(session_id))
        response.extend(len(payload).to_bytes(4, "big"))
        response.extend(payload)
    elif event in (EVENT_TTSSentenceStart, EVENT_TTSSentenceEnd, EVENT_TTS_SUBTITLE):
        assert session_id is not None
        assert payload_json is not None
        response.extend(_content(session_id))
        response.extend(_content(payload_json))
    else:
        raise AssertionError(f"unsupported mock response event: {event}")

    return bytes(response)


def _error_response(code: int, message: str) -> bytes:
    payload = message.encode("utf-8")
    return (
        Header(message_type=ERROR_INFORMATION).as_bytes()
        + code.to_bytes(4, "big")
        + len(payload).to_bytes(4, "big")
        + payload
    )


def _client_message(message: bytes) -> tuple[int, str | None, dict[str, Any] | None]:
    event = int.from_bytes(message[4:8], "big")
    offset = 8
    session_id: str | None = None

    if event in (EVENT_StartSession, EVENT_FinishSession, EVENT_TaskRequest):
        session_id_size = int.from_bytes(message[offset : offset + 4], "big")
        offset += 4
        session_id = message[offset : offset + session_id_size].decode("utf-8")
        offset += session_id_size

    payload: dict[str, Any] | None = None
    if event in (EVENT_Start_Connection, EVENT_StartSession, EVENT_TaskRequest):
        payload_size = int.from_bytes(message[offset : offset + 4], "big")
        offset += 4
        payload = json.loads(message[offset : offset + payload_size])

    return event, session_id, payload


class TTSSubtitleServer:
    """Return a TTSSubtitle event using the ByteDance V3 wire format."""

    def __init__(self) -> None:
        self.ready = threading.Event()
        self.loop: asyncio.AbstractEventLoop | None = None
        self.server: websockets.Server | None = None
        self.port: int | None = None
        self.audio_params: dict[str, Any] = {}
        self.additions: list[dict[str, Any]] = []
        self.speakers: list[str] = []
        self.task_request_count = 0
        self.session_task_count = 0
        self.received_texts: list[str] = []
        self.connection_attempts = 0

    @property
    def uri(self) -> str:
        assert self.port is not None
        return f"ws://127.0.0.1:{self.port}"

    def start(self) -> None:
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()
        assert self.ready.wait(timeout=0.5), "mock server did not start"

    def stop(self) -> None:
        if self.loop is not None:
            shutdown = asyncio.run_coroutine_threadsafe(self._shutdown(), self.loop)
            shutdown.result(timeout=0.3)
            self.loop.call_soon_threadsafe(self.loop.stop)
        self.thread.join(timeout=0.3)

    def _run(self) -> None:
        self.loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.loop)
        self.server = self.loop.run_until_complete(self._start_server())
        assert self.server.sockets is not None
        self.port = self.server.sockets[0].getsockname()[1]
        self.ready.set()
        self.loop.run_forever()
        self.loop.close()

    async def _shutdown(self) -> None:
        if self.server is not None:
            for websocket in list(self.server.connections):
                await websocket.close()
            self.server.close()
            await self.server.wait_closed()

    async def _start_server(self) -> websockets.Server:
        return await websockets.serve(self._handler, "127.0.0.1", 0)

    async def _handler(self, websocket, *_args: Any) -> None:
        try:
            async for raw_message in websocket:
                event, session_id, payload = _client_message(raw_message)

                if event == EVENT_Start_Connection:
                    self.connection_attempts += 1
                    response_event = (
                        EVENT_ConnectionFailed
                        if self.connection_attempts == 1
                        else EVENT_ConnectionStarted
                    )
                    await websocket.send(_server_response(response_event))
                elif event == EVENT_StartSession:
                    assert session_id is not None
                    assert payload is not None
                    self.session_task_count = 0
                    self.audio_params = payload["req_params"]["audio_params"]
                    self.additions.append(
                        json.loads(payload["req_params"]["additions"]),
                    )
                    await websocket.send(
                        _server_response(EVENT_SessionStarted, session_id=session_id),
                    )
                elif event == EVENT_TaskRequest:
                    assert session_id is not None
                    assert payload is not None
                    text = payload["req_params"]["text"]
                    self.received_texts.append(text)
                    if text == "illegal input":
                        await websocket.send(
                            _error_response(3011, "illegal input text"),
                        )
                        continue
                    if text.startswith("request "):
                        await websocket.send(
                            _server_response(
                                EVENT_TTSResponse,
                                session_id=session_id,
                                payload=b"\x01\x00" * 160,
                            ),
                        )
                        continue
                    self.speakers.append(payload["req_params"]["speaker"])
                    self.task_request_count += 1
                    if self.session_task_count == 0:
                        await websocket.send(
                            _server_response(
                                EVENT_TTSSentenceStart,
                                session_id=session_id,
                                payload_json="{}",
                            ),
                        )
                    self.session_task_count += 1
                    await websocket.send(
                        _server_response(
                            EVENT_TTSResponse,
                            session_id=session_id,
                            payload=PCM_CHUNK,
                        ),
                    )
                    subtitle_event = (
                        EVENT_TTS_SUBTITLE
                        if payload["req_params"]["speaker"]
                        == "zh_female_vv_uranus_bigtts"
                        else EVENT_TTSSentenceEnd
                    )
                    await websocket.send(
                        _server_response(
                            subtitle_event,
                            session_id=session_id,
                            payload_json=json.dumps(self._subtitle_payload()),
                        ),
                    )
                elif event == EVENT_FinishSession:
                    assert session_id is not None
                    await websocket.send(
                        _server_response(
                            EVENT_SessionFinished,
                            session_id=session_id,
                        ),
                    )
                elif event == EVENT_FinishConnection:
                    continue
        except websockets.ConnectionClosed:
            return

    def _subtitle_payload(self) -> dict[str, Any]:
        if self.task_request_count == 1:
            return {
                "text": "你好，",
                "words": [
                    {"word": "你好", "startTime": 0.095, "endTime": 0.395},
                    {"word": "，", "startTime": 0.395, "endTime": 0.495},
                ],
            }
        if self.task_request_count == 2:
            return {
                "text": "第二句。",
                "words": [
                    {"word": "第二句", "startTime": 0.895, "endTime": 1.195},
                    {"word": "。", "startTime": 1.195, "endTime": 1.595},
                ],
            }
        return {
            "text": "这是2.0。",
            "words": [
                {"word": "这是", "startTime": 7.5, "endTime": 7.8},
                {"word": "2.0。", "startTime": 7.8, "endTime": 8.2},
            ],
        }


def _text_input(
    request_id: str,
    text: str,
    text_input_end: bool,
    turn_seq_id: int,
    turn_id: int,
) -> Data:
    data = Data.create("tts_text_input")
    data.set_property_from_json(
        "",
        json.dumps(
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": text_input_end,
                "metadata": {
                    "turn_id": turn_id,
                    "turn_seq_id": turn_seq_id,
                    "stream_id": 1,
                    "user_id": "test_user",
                },
            },
        ),
    )
    return data


def _text_result(data: Data) -> TTSTextResult:
    payload, error = data.get_property_to_json("")
    assert error is None
    return TTSTextResult.model_validate_json(payload)


class CombinedTTSTester(ExtensionTester):
    def __init__(self, server: TTSSubtitleServer) -> None:
        super().__init__()
        self.server = server
        self.audio_end_events: list[dict[str, int | str]] = []
        self.errors: list[dict[str, Any]] = []
        self._timeout: threading.Timer | None = None
        self._connected = False
        self._requests_sent = False
        self.subtitles: list[TTSTextResult] = []
        self.final_results: list[TTSTextResult] = []
        self.audio_timestamps: list[int] = []
        self.caption_ends = 0
        self.caption_started = False
        self._finish_started = False

    def on_audio_frame(self, ten_env: TenEnvTester, frame: AudioFrame) -> None:
        if self.caption_started:
            self.audio_timestamps.append(frame.get_timestamp())

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        if self.caption_started and data.get_name() == "tts_text_result":
            result = _text_result(data)
            if result.text:
                self.subtitles.append(result)
            elif result.text_result_end:
                self.final_results.append(result)
            self._stop_if_complete(ten_env)
            return
        if self.caption_started and data.get_name() == "tts_audio_end":
            request_id, error = data.get_property_string("request_id")
            if error is None and request_id.startswith("request-tts"):
                self.caption_ends += 1
                if self.caption_ends == 1:
                    self._update_config(
                        ten_env,
                        "seed-tts-2.0",
                        "zh_female_vv_uranus_bigtts",
                    )
                self._stop_if_complete(ten_env)
                return
        if data.get_name() == "error":
            payload, error = data.get_property_to_json("")
            if error is None:
                self.errors.append(json.loads(payload))
                self._send_requests_if_ready(ten_env)
                self._stop_if_complete(ten_env)
        elif data.get_name() == "connection_status_changed":
            payload, error = data.get_property_to_json("")
            if error is None and json.loads(payload)["current"] == "connected":
                self._connected = True
                self._send_requests_if_ready(ten_env)
        elif data.get_name() == "tts_audio_end":
            request_id, request_error = data.get_property_string("request_id")
            reason, reason_error = data.get_property_int("reason")
            if request_error is None and reason_error is None:
                self.audio_end_events.append(
                    {"request_id": request_id, "reason": reason},
                )
                self._stop_if_complete(ten_env)

    def _send_requests_if_ready(self, ten_env: TenEnvTester) -> None:
        error_codes = {error["vendor_info"]["code"] for error in self.errors}
        if (
            self._requests_sent
            or not self._connected
            or "CONNECTION_FAILED" not in error_codes
        ):
            return
        self._requests_sent = True
        for text, request_id, ended in (
            ("request three", "request-3", False),
            ("illegal input", "request-3", True),
            ("request four", "request-4", True),
            ("request five", "request-5", True),
        ):
            data = _text_input(request_id, text, ended, 1, 1)
            ten_env.send_data(data)

    def on_start(self, ten_env: TenEnvTester) -> None:
        def fail_on_timeout() -> None:
            ten_env.stop_test(
                TenError.create(TenErrorCode.ErrorCodeGeneric, "TTS test timed out"),
            )

        self._timeout = threading.Timer(0.8, fail_on_timeout)
        self._timeout.start()
        ten_env.on_start_done()

    def _stop_if_complete(self, ten_env: TenEnvTester) -> None:
        if self.caption_started:
            if (
                len(self.subtitles) == 3
                and len(self.final_results) == 2
                and self.caption_ends == 2
                and not self._finish_started
            ):
                self._finish_started = True
                if self._timeout is not None:
                    self._timeout.cancel()
                threading.Timer(0.03, ten_env.stop_test).start()
            return
        observed_ids = {event["request_id"] for event in self.audio_end_events}
        error_codes = {error["vendor_info"]["code"] for error in self.errors}
        if {"CONNECTION_FAILED", "3011"}.issubset(error_codes) and {
            "request-3",
            "request-4",
            "request-5",
        }.issubset(observed_ids):
            self.caption_started = True
            self._update_config(
                ten_env,
                "seed-tts-1.0",
                "zh_female_shuangkuaisisi_moon_bigtts",
            )

    def _update_config(
        self,
        ten_env: TenEnvTester,
        resource_id: str,
        speaker: str,
    ) -> None:
        update = Cmd.create("update_configs")
        update.set_property_from_json(
            "",
            json.dumps({"resource_id": resource_id, "params": {"speaker": speaker}}),
        )
        ten_env.send_cmd(update, self._on_config_updated)

    def _on_config_updated(
        self,
        ten_env: TenEnvTester,
        result: CmdResult | None,
        error: TenError | None,
    ) -> None:
        if (
            error is not None
            or result is None
            or result.get_status_code() != StatusCode.OK
        ):
            ten_env.stop_test(
                TenError.create(TenErrorCode.ErrorCodeGeneric, "config update failed"),
            )
            return
        if self.caption_ends == 0:
            for text, text_input_end, turn_seq_id in (
                ("你好，", False, 0),
                ("第二句。", True, 1),
            ):
                ten_env.send_data(
                    _text_input(
                        "request-tts1-subtitle",
                        text,
                        text_input_end,
                        turn_seq_id,
                        1,
                    ),
                )
        else:
            ten_env.send_data(
                _text_input("request-tts2-subtitle", "这是2.0。", True, 0, 2),
            )


def test_errors_and_subtitle_events() -> None:
    # TEN's native tester cannot be initialized twice in one pytest process.
    server = TTSSubtitleServer()
    server.start()
    try:
        tester = CombinedTTSTester(server)
        tester.set_test_mode_single(
            "bytedance_tts_duplex",
            json.dumps(
                {
                    "app_id": "fake-app-id",
                    "token": "fake-token",
                    "url": server.uri,
                    "speaker": "mock-speaker",
                    "sample_rate": 16000,
                    "enable_words": True,
                    "params": {},
                },
            ),
        )
        error = tester.run()
        assert error is None, error.error_message()
        counts = collections.Counter(
            event["request_id"] for event in tester.audio_end_events
        )
        assert counts == {"request-3": 1, "request-4": 1, "request-5": 1}
        reasons = {
            event["request_id"]: event["reason"] for event in tester.audio_end_events
        }
        assert reasons["request-3"] == TTSAudioEndReason.ERROR.value
        assert reasons["request-4"] == TTSAudioEndReason.REQUEST_END.value
        assert reasons["request-5"] == TTSAudioEndReason.REQUEST_END.value
        assert server.received_texts[:2] == ["request three", "illegal input"]
        error_counts = collections.Counter(
            error["vendor_info"]["code"] for error in tester.errors
        )
        assert error_counts == {"CONNECTION_FAILED": 1, "3011": 1}
        connection_error = next(
            error
            for error in tester.errors
            if error["vendor_info"]["code"] == "CONNECTION_FAILED"
        )
        assert connection_error["id"] == ""
        vendor_error = next(
            error for error in tester.errors if error["vendor_info"]["code"] == "3011"
        )
        assert vendor_error["id"] == "request-3"
        assert vendor_error["vendor_info"]["message"] == "illegal input text"
        assert server.audio_params["enable_subtitle"] is True
        assert server.audio_params["enable_timestamp"] is True
        assert server.additions
        assert all(
            additions["cache_config"]["use_cache"] is False
            for additions in server.additions
        )
        assert server.speakers == [
            "zh_female_shuangkuaisisi_moon_bigtts",
            "zh_female_shuangkuaisisi_moon_bigtts",
            "zh_female_vv_uranus_bigtts",
        ]
        assert [subtitle.text for subtitle in tester.subtitles] == [
            "你好，",
            "第二句。",
            "这是2.0。",
        ]
        first, second, third = tester.subtitles
        assert len(tester.audio_timestamps) == 3
        assert tester.audio_timestamps[1] - tester.audio_timestamps[0] == 800
        assert first.start_ms == tester.audio_timestamps[0] + 95
        assert second.start_ms == tester.audio_timestamps[1] + 95
        assert second.start_ms - first.start_ms == 800
        assert first.words[-1].start_ms < second.words[0].start_ms
        assert third.start_ms == tester.audio_timestamps[2]
        assert [subtitle.metadata["turn_seq_id"] for subtitle in tester.subtitles] == [
            0,
            1,
            0,
        ]
        assert [result.metadata["turn_seq_id"] for result in tester.final_results] == [
            2,
            1,
        ]
        assert [subtitle.metadata["turn_status"] for subtitle in tester.subtitles] == [
            0,
            0,
            0,
        ]
        assert [result.metadata["turn_status"] for result in tester.final_results] == [
            1,
            1,
        ]
        assert len(tester.subtitles) == 3
        assert len(tester.final_results) == 2
        assert tester.caption_ends == 2
    finally:
        server.stop()
