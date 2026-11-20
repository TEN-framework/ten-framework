import asyncio
import base64
import json
from datetime import datetime, timedelta
from types import SimpleNamespace
from unittest.mock import AsyncMock, Mock

import httpx
import pytest
from ten_ai_base import tts2_http
from ten_ai_base.message import TTSAudioEndReason
from ten_runtime import Data

from ten_packages.extension.gnani_tts_python.config import GnaniTTSConfig
from ten_packages.extension.gnani_tts_python.extension import GnaniTTSExtension


def message(name, **properties):
    data = Data.create(name)
    data.set_property_from_json(None, json.dumps(properties))
    return data


@pytest.mark.asyncio
@pytest.mark.parametrize(
    "model,voice", [("timbre-v2.5", "Nalini"), ("timbre-v3.0", "Shivani")]
)
@pytest.mark.parametrize("scenario", ["before_audio", "during_audio", "error"])
async def test_stream_lifecycle_and_next_request(
    model, voice, scenario, monkeypatch, tmp_path
):
    clock = datetime(2025, 1, 1)
    monkeypatch.setattr(tts2_http, "datetime", SimpleNamespace(now=lambda: clock))
    waiting = asyncio.Event()
    first_closed = asyncio.Event()
    first_pcm = b"\x01\x02" * 1600
    next_pcm = b"\x03\x04" * 800

    class Stream(httpx.AsyncByteStream):
        def __init__(self, request_id):
            self.request_id = request_id

        async def __aiter__(self):
            nonlocal clock
            first = self.request_id == "first"
            if not first or scenario != "before_audio":
                clock += timedelta(milliseconds=30)
                encoded = base64.b64encode(first_pcm if first else next_pcm)
                yield b'event: chunk\ndata: {"audio":"' + encoded + b'"}\n\n'
            clock += timedelta(milliseconds=250)
            if first and scenario != "error":
                waiting.set()
                await asyncio.Event().wait()
            if first:
                yield b"event: error\ndata: {}\n\n"
            else:
                yield b"event: complete\ndata: {}\n\n"

        async def aclose(self):
            if self.request_id == "first":
                first_closed.set()

    def handle(request):
        assert json.loads(request.content)["model"] == model
        return httpx.Response(200, stream=Stream(request.extensions["ten_request_id"]))

    env = Mock(send_data=AsyncMock(), send_audio_frame=AsyncMock())
    ext = GnaniTTSExtension("gnani")
    ext.ten_env = env
    ext.config = GnaniTTSConfig(
        params=dict(api_key="unit-test", model=model, voice=voice, sample_rate=16000),
        dump=True,
        dump_path=str(tmp_path),
    )
    ext.client = await ext.create_client(ext.config, env)
    await ext.client.http.aclose()
    ext.client.http = httpx.AsyncClient(transport=httpx.MockTransport(handle))
    worker = asyncio.create_task(ext._process_input_queue(env))

    async def submit(request_id):
        await ext.on_data(
            env,
            message(
                "tts_text_input",
                request_id=request_id,
                text="Hello",
                text_input_end=True,
                metadata={"turn": request_id},
            ),
        )

    try:
        await submit("first")
        if scenario != "error":
            await asyncio.wait_for(waiting.wait(), 2)
            await ext.on_data(env, message("tts_flush", flush_id="interrupt"))
        await asyncio.wait_for(first_closed.wait(), 2)
        await submit("next")
        await ext.input_queue.put(None)
        await asyncio.wait_for(worker, 2)
    finally:
        if not worker.done():
            await ext._cancel_current_task()
            await ext.input_queue.put(None)
            await worker
        await ext.client.clean()

    events = [
        (data.get_name(), json.loads(data.get_property_to_json("")[0]))
        for (data,), _ in env.send_data.await_args_list
    ]
    starts = [body["request_id"] for name, body in events if name == "tts_audio_start"]
    ends = [
        (
            body["request_id"],
            body["reason"],
            body["request_total_audio_duration_ms"],
            body["request_event_interval_ms"],
        )
        for name, body in events
        if name == "tts_audio_end"
    ]
    errors = [body["id"] for name, body in events if name == "error"]
    flushes = [body["flush_id"] for name, body in events if name == "tts_flush_end"]
    frames = [
        (
            json.loads(frame.get_property_to_json("metadata")[0])["turn"],
            bytes(frame.get_buf()),
        )
        for (frame,), _ in env.send_audio_frame.await_args_list
    ]
    first_audible = scenario != "before_audio"
    expected_starts = ["first"] if first_audible else []
    expected_ends = []
    if first_audible:
        reason = (
            TTSAudioEndReason.ERROR
            if scenario == "error"
            else TTSAudioEndReason.INTERRUPTED
        )
        expected_ends.append(("first", reason, 100, 250))
    assert starts == expected_starts + ["next"]
    assert ends == expected_ends + [("next", TTSAudioEndReason.REQUEST_END, 50, 250)]
    assert frames == ([("first", first_pcm)] if first_audible else []) + [
        ("next", next_pcm)
    ]
    assert errors == (["first"] if scenario == "error" else [])
    assert flushes == ([] if scenario == "error" else ["interrupt"])
    first_dump = tmp_path / "gnani_dump_first.pcm"
    if first_audible:
        assert first_dump.read_bytes() == first_pcm
    else:
        assert not first_dump.exists() or first_dump.stat().st_size == 0
    assert (tmp_path / "gnani_dump_next.pcm").read_bytes() == next_pcm
