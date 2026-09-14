import asyncio
import json
from types import SimpleNamespace
from unittest.mock import AsyncMock

import aiohttp
import pytest
from ten_packages.extension.rtzr_asr_python.client import RTZRClient
from ten_packages.extension.rtzr_asr_python.extension import RTZRASRExtension
from ten_runtime import (
    AsyncExtensionTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)

from .test_config import config


class FakeSocket:
    def __init__(self):
        self.closed = False
        self.close_code = 1000
        self.messages = asyncio.Queue()
        self.controls = []
        self.audio = bytearray()
        self.segment_start = 0

    def __aiter__(self):
        return self

    async def __anext__(self):
        message = await self.messages.get()
        if message is None:
            raise StopAsyncIteration
        return SimpleNamespace(
            type=aiohttp.WSMsgType.TEXT, data=json.dumps(message)
        )

    async def send_bytes(self, audio):
        self.audio.extend(audio)

    async def send_json(self, message):
        self.controls.append(message)
        end = len(self.audio) // 32
        await self.messages.put(
            {
                "seq": len(self.controls),
                "start_at": self.segment_start,
                "duration": end - self.segment_start,
                "final": True,
                "alternatives": [
                    {
                        "text": "hello world",
                        "words": [
                            {
                                "text": "hello",
                                "start_at": 0,
                                "duration": end - self.segment_start,
                            }
                        ],
                    }
                ],
            }
        )
        self.segment_start = end

    async def send_str(self, message):
        self.controls.append(message)
        await self.close()

    async def close(self):
        self.closed = True
        await self.messages.put(None)


class RecognitionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.results = []
        self.metrics = []
        self.errors = []
        self.ends = []
        self.statuses = []
        self.sent_audio_bytes = 0
        self.done = asyncio.Event()
        self.task = None
        self.cycles = 3
        self.audio = b"\x01\x02" * 1600
        self.audio_samples = None
        self.expected_language = "ko-KR"

    async def on_start(self, ten_env):
        self.task = asyncio.create_task(self.run_audio(ten_env))

    async def run_audio(self, ten_env):
        try:
            for cycle in range(self.cycles):
                self.done.clear()
                audio = (
                    self.audio_samples[cycle]
                    if self.audio_samples
                    else self.audio
                )
                for offset in range(0, len(audio), 320):
                    if len(audio) >= 32000 * 300 and offset % (32000 * 30) == 0:
                        print(
                            f"[stream] sent_audio_seconds={offset // 32000}",
                            flush=True,
                        )
                    chunk = audio[offset : offset + 320]
                    frame = AudioFrame.create("pcm_frame")
                    frame.set_property_from_json(
                        "metadata", json.dumps({"session_id": "session"})
                    )
                    frame.alloc_buf(len(chunk))
                    buf = frame.lock_buf()
                    buf[:] = chunk
                    frame.unlock_buf(buf)
                    await ten_env.send_audio_frame(frame)
                    self.sent_audio_bytes += len(chunk)
                    await asyncio.sleep(0.01)
                finalize = Data.create("asr_finalize")
                finalize.set_property_from_json(
                    None,
                    json.dumps(
                        {
                            "finalize_id": str(cycle),
                            "metadata": {"session_id": "session"},
                        }
                    ),
                )
                await ten_env.send_data(finalize)
                await asyncio.wait_for(self.done.wait(), 10)
            ten_env.stop_test()
        except Exception as exc:
            ten_env.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric,
                    str(exc) or type(exc).__name__,
                )
            )

    async def on_data(self, ten_env, data):
        raw, _ = data.get_property_to_json(None)
        payload = json.loads(raw)
        name = data.get_name()
        if name == "asr_result":
            self.results.append(payload)
        elif name == "metrics":
            self.metrics.append(payload)
        elif name == "connection_status_changed":
            self.statuses.append(payload)
        elif name == "error":
            self.errors.append(payload)
            ten_env.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric, payload["message"]
                )
            )
        elif name == "asr_finalize_end":
            self.ends.append(payload)
            self.done.set()

    async def on_stop(self, ten_env):
        if self.task:
            self.task.cancel()
            await asyncio.gather(self.task, return_exceptions=True)


@pytest.mark.parametrize(
    "model,language",
    [("sommers_ko", "ko-KR"), ("sommers_ja", "ja-JP"), ("sommers_en", "en-US")],
)
def test_runtime_results_finalize_dump(monkeypatch, tmp_path, model, language):
    sockets = []

    async def connect(_self):
        ws = FakeSocket()
        sockets.append(ws)
        return ws

    monkeypatch.setattr(RTZRClient, "connect", connect)
    tester = RecognitionTester()
    value = config(model_name=model).model_dump()
    value.update(dump=True, dump_path=str(tmp_path))
    tester.set_test_mode_single("rtzr_asr_python", json.dumps(value))
    error = tester.run()
    assert error is None, error.error_message() if error else ""
    assert not tester.errors
    assert len(tester.results) == 3
    assert len({item["id"] for item in tester.results}) == 3
    for index, result in enumerate(tester.results):
        assert result["language"] == language
        assert result["metadata"]["session_id"] == "session"
        assert result["final"] is True
        assert result["start_ms"] == index * 100
        assert result["duration_ms"] == 100
        assert result["words"][0]["word"] == "hello"
    assert [end["finalize_id"] for end in tester.ends] == ["0", "1", "2"]
    assert tester.metrics
    assert len(sockets) == 1
    assert sockets[0].controls == [{"type": "Finalize"}] * 3 + ["EOS"]
    assert sockets[0].closed
    assert (tmp_path / "rtzr_asr_in.pcm").read_bytes() == tester.audio * 3


def extension():
    ext = RTZRASRExtension("test")
    ext.config = config()
    ext.ten_env = SimpleNamespace(
        log_info=lambda *a, **k: None,
        log_debug=lambda *a, **k: None,
        log_warn=lambda *a, **k: None,
        send_data=AsyncMock(),
    )
    ext.client = RTZRClient(ext.config)
    return ext


async def test_finalization_timeout_does_not_report_success():
    ext = extension()
    ext.ws = FakeSocket()
    ext.ws.send_json = AsyncMock()
    ext.config.finalize_timeout = 0.01
    ext._sent_bytes = 320
    await ext.finalize("session")
    names = [
        call.args[0].get_name() for call in ext.ten_env.send_data.call_args_list
    ]
    assert names == ["error"]


async def test_fatal_auth_does_not_retry():
    ext = extension()
    ext.client.connect = AsyncMock(
        side_effect=aiohttp.ClientResponseError(None, (), status=401)
    )
    await ext.start_connection()
    assert ext._fatal
    assert ext._retry is None


async def test_stop_cancels_retry():
    ext = extension()
    ext.client.connect = AsyncMock(side_effect=OSError())
    await ext.start_connection()
    task = ext._retry
    await ext.on_stop(ext.ten_env)
    assert task.done()
    assert ext.client.connect.await_count == 1


async def test_reconnect_budget(monkeypatch):
    ext = extension()
    ext.client.connect = AsyncMock(side_effect=OSError())
    monkeypatch.setattr(asyncio, "sleep", AsyncMock())
    await ext.start_connection()
    await ext._retry
    assert ext._fatal
    assert ext.client.connect.await_count == 6


async def test_result_offsets_and_retry_reset():
    ext = extension()
    ext._attempts = 4
    ext._connection_offset_ms = 500
    await ext._result(
        {
            "final": True,
            "start_at": 100,
            "duration": 50,
            "alternatives": [{"text": "test"}],
        }
    )
    assert ext._attempts == 0
    data = ext.ten_env.send_data.call_args.args[0]
    assert json.loads(data.get_property_to_json(None)[0])["start_ms"] == 600


def audio_frame(audio=b"\x01\x02" * 160):
    frame = AudioFrame.create("pcm_frame")
    frame.alloc_buf(len(audio))
    buf = frame.lock_buf()
    buf[:] = audio
    frame.unlock_buf(buf)
    frame.set_property_from_json("metadata", '{"session_id":"buffered"}')
    return frame


async def test_buffer_flushes_on_connection_without_new_audio():
    ext = extension()
    frame = audio_frame()
    await ext._handle_audio_frame(ext.ten_env, frame)
    assert ext._pending_bytes == 320
    ws = FakeSocket()
    ext.client.connect = AsyncMock(return_value=ws)
    await ext.start_connection()
    assert bytes(ws.audio) == bytes(frame.get_buf())
    assert ext.metadata["session_id"] == "buffered"
    assert ext._pending_bytes == 0
    await ext.on_stop(ext.ten_env)


async def test_failed_send_retains_frame_for_reconnect():
    ext = extension()
    ws = FakeSocket()
    ext.ws = ws
    ws.send_bytes = AsyncMock(side_effect=OSError())
    await ext._handle_audio_frame(ext.ten_env, audio_frame())
    assert ext._pending_bytes == 320
    await ext.on_stop(ext.ten_env)


async def test_buffer_is_bounded():
    ext = extension()
    from ten_ai_base.asr import ASRBufferConfigModeKeep

    ext.buffer_strategy = lambda: ASRBufferConfigModeKeep(byte_limit=640)
    for _ in range(3):
        await ext._handle_audio_frame(ext.ten_env, audio_frame())
    assert ext._pending_bytes == 640
    assert len(ext._pending) == 2
    assert ext.ten_env.send_data.call_args.args[0].get_name() == "error"


async def test_settled_auto_final_has_explicit_local_completion():
    ext = extension()
    ext.ws = FakeSocket()
    ext.ws.send_json = AsyncMock()
    ext.config.finalize_timeout = 0.01
    ext._sent_bytes = 640
    ext._received_result = True
    ext._final_bytes = 320
    await ext.finalize("session")
    names = [
        call.args[0].get_name() for call in ext.ten_env.send_data.call_args_list
    ]
    assert names == ["metrics", "asr_finalize_end"]
    assert not ext.ws.closed


async def test_pending_hypothesis_cannot_complete_by_timeout():
    ext = extension()
    ext.ws = FakeSocket()
    ext.ws.send_json = AsyncMock()
    ext.config.finalize_timeout = 0.01
    ext._sent_bytes = 320
    ext._received_result = True
    ext._pending_result = True
    await ext.finalize("session")
    assert ext.ten_env.send_data.call_args.args[0].get_name() == "error"


@pytest.mark.parametrize(
    "payload",
    [
        [],
        {"final": "false"},
        {"final": True, "start_at": -1, "duration": 1, "alternatives": []},
    ],
)
async def test_reject_invalid_vendor_payload(payload):
    ext = extension()
    with pytest.raises(ValueError):
        await ext._result(payload)


@pytest.mark.parametrize("words", [None, {}, [None], ["word"]])
async def test_reject_invalid_words_without_changing_result_state(words):
    ext = extension()
    with pytest.raises(ValueError):
        await ext._result(
            {
                "final": True,
                "start_at": 0,
                "duration": 1,
                "alternatives": [{"text": "hello", "words": words}],
            }
        )
    assert not ext._received_result


async def test_receive_close_reconnects_and_shutdown_drains_tasks():
    ext = extension()
    first, second = FakeSocket(), FakeSocket()
    ext.client.connect = AsyncMock(side_effect=[first, second])
    await ext.start_connection()
    receiver = ext._receiver
    await first.close()
    await asyncio.wait_for(receiver, 1)
    retry = ext._retry
    await asyncio.wait_for(retry, 2)
    assert ext.ws is second
    assert ext.client.connect.await_count == 2
    statuses = [
        json.loads(call.args[0].get_property_to_json(None)[0])["current"]
        for call in ext.ten_env.send_data.call_args_list
        if call.args[0].get_name() == "connection_status_changed"
    ]
    assert statuses == [
        "connecting",
        "connected",
        "disconnected",
        "connecting",
        "connected",
    ]
    await ext.on_stop(ext.ten_env)
    assert second.closed
    assert ext._receiver is None


class ConfigErrorTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.error = None

    async def on_data(self, ten_env, data):
        if data.get_name() == "error":
            self.error = json.loads(data.get_property_to_json(None)[0])
            ten_env.stop_test()


def test_runtime_invalid_config_is_fatal_and_redacted():
    tester = ConfigErrorTester()
    tester.set_timeout(5 * 1000 * 1000)
    tester.set_test_mode_single(
        "rtzr_asr_python",
        json.dumps({"params": {"client_secret": "do-not-log-this-secret"}}),
    )
    result = tester.run()
    assert result is None
    assert tester.error["code"] == -1000
    assert "do-not-log-this-secret" not in json.dumps(tester.error)
