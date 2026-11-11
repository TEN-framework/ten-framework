import asyncio
import json
from contextlib import asynccontextmanager
from datetime import datetime, timedelta
from types import SimpleNamespace
from unittest.mock import AsyncMock, MagicMock, patch

import aiohttp
import pytest
from aiohttp import web
from aiohttp.test_utils import TestServer
from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2 import RequestState
from ten_runtime import Data

from typecast_tts_python.config import TypecastTTSConfig
from typecast_tts_python.extension import TypecastTTSExtension
from typecast_tts_python.pcm import StreamingWavToPcm16
from typecast_tts_python.typecast_tts import TypecastTTSClient
from test_typecast_tts import WAV_HEADER

PCM = b"\x01\x02" * 3200
KEY = "fixture-key-not-a-real-credential"
TEXT = "Private fixture sentence, not customer data."


@pytest.fixture
def local_extension(tmp_path):
    @asynccontextmanager
    async def start(handler, params=None):
        events, requests, metrics = [], [], []
        env = MagicMock()
        now = datetime(2026, 1, 1)

        async def send_data(data):
            body, _ = data.get_property_to_json()
            if data.get_name() == "metrics":
                metrics.append(json.loads(body))
            else:
                events.append((data.get_name(), json.loads(body)))

        async def send_audio(frame):
            nonlocal now
            buffer = frame.lock_buf()
            try:
                events.append(
                    (
                        "pcm",
                        {
                            "request_id": ext.current_request_id,
                            "audio": bytes(buffer),
                        },
                    )
                )
            finally:
                frame.unlock_buf(buffer)
            now += timedelta(milliseconds=25)

        async def serve(request):
            body = await request.json()
            requests.append(body)
            return await handler(request, body)

        env.send_data = AsyncMock(side_effect=send_data)
        env.send_audio_frame = AsyncMock(side_effect=send_audio)
        app = web.Application()
        app.router.add_post("/v1/text-to-speech/stream", serve)
        async with TestServer(app) as server:
            ext = TypecastTTSExtension("test")
            ext.ten_env = env
            ext.config = TypecastTTSConfig(
                dump=True,
                dump_path=str(tmp_path),
                chunk_size=128,
                params={
                    "api_key": KEY,
                    "voice_id": "fixture-voice",
                    "model": "ssfm-v31",
                    "url": str(server.make_url("/")),
                    **(params or {}),
                },
            )
            ext.config.update_params()
            ext.config.validate()
            ext.client = TypecastTTSClient(ext.config, env)

            async def send(name, payload):
                data = Data.create(name)
                data.set_property_from_json(None, json.dumps(payload))
                await ext.on_data(env, data)

            async def input_text(request_id, text, final=True):
                await send(
                    "tts_text_input",
                    {
                        "request_id": request_id,
                        "text": text,
                        "text_input_end": final,
                    },
                )

            async def finished(request_id):
                async def wait():
                    while (
                        ext.request_states.get(request_id)
                        != RequestState.COMPLETED
                    ):
                        await asyncio.sleep(0.001)

                await asyncio.wait_for(wait(), 2)
                assert request_id not in ext.recorder_map
                assert ext._processing_request_id is None
                assert not ext.client._client.session.connector._acquired

            async def pcm_frames(count):
                async def wait():
                    while sum(name == "pcm" for name, _ in events) < count:
                        await asyncio.sleep(0.001)

                await asyncio.wait_for(wait(), 1)

            with patch("ten_ai_base.tts2_http.datetime") as clock:
                clock.now.side_effect = lambda: now
                loop = asyncio.create_task(ext._process_input_queue(env))
                try:
                    yield SimpleNamespace(
                        ext=ext,
                        env=env,
                        events=events,
                        requests=requests,
                        metrics=metrics,
                        send=send,
                        input=input_text,
                        finished=finished,
                        pcm_frames=pcm_frames,
                    )
                finally:
                    await ext.input_queue.put(None)
                    loop.cancel()
                    await asyncio.wait_for(
                        asyncio.gather(loop, return_exceptions=True), 2
                    )
                    session = (
                        ext.client._client.session
                        if ext.client._client
                        else None
                    )
                    await ext.client.clean()
                    if session:
                        assert session.closed

    return start


def assert_terminal(harness, tmp_path, request_id, reason):
    own = [
        (name, body)
        for name, body in harness.events
        if body.get("request_id") == request_id
    ]
    ends = [body for name, body in own if name == "tts_audio_end"]
    chunks = [body["audio"] for name, body in own if name == "pcm"]
    assert len(ends) == 1
    assert ends[0]["reason"] == reason.value
    assert (
        ends[0]["request_total_audio_duration_ms"]
        == sum(map(len, chunks)) // 64
    )
    assert ends[0]["request_event_interval_ms"] == len(chunks) * 25
    assert sum(name == "tts_audio_start" for name, _ in own) == bool(chunks)
    path = tmp_path / f"typecast_dump_{request_id}.pcm"
    assert (path.read_bytes() if path.exists() else b"") == b"".join(chunks)
    assert request_id not in harness.ext.recorder_map


@pytest.mark.parametrize("failure", ["timeout", "disconnect"])
@pytest.mark.parametrize("partial", [False, True])
def test_real_transport_failure_then_recovery(
    local_extension, tmp_path, failure, partial
):
    async def run():
        stop = asyncio.Event()
        transports = []

        async def serve(request, body):
            transports.append(request.transport)
            if body["text"] == "recovery":
                return web.Response(
                    body=WAV_HEADER + PCM, content_type="audio/wav"
                )
            if partial:
                response = web.StreamResponse(
                    headers={"Content-Type": "audio/wav"}
                )
                await response.prepare(request)
                await response.write(WAV_HEADER + PCM)
                await h.pcm_frames(1)
            if failure == "disconnect":
                request.transport.abort()
                return web.Response()
            try:
                await asyncio.wait_for(stop.wait(), 1)
            except asyncio.TimeoutError:
                pass
            return web.Response()

        timeout = aiohttp.ClientTimeout(sock_connect=1, sock_read=0.05)
        try:
            async with local_extension(serve) as h:
                with patch(
                    "typecast.async_client.aiohttp.ClientTimeout",
                    return_value=timeout,
                ):
                    await h.input("failed", "first")
                    await h.finished("failed")
                assert_terminal(h, tmp_path, "failed", TTSAudioEndReason.ERROR)
                errors = [body for name, body in h.events if name == "error"]
                assert len(errors) == 1
                assert errors[0]["code"] == 1000
                assert errors[0]["vendor_info"]["vendor"] == "typecast"
                assert (
                    "Timeout" if failure == "timeout" else "Error"
                ) in errors[0]["message"]
                assert any(name == "pcm" for name, _ in h.events) == partial
                assert transports[0].is_closing()
                await h.input("recovery", "recovery")
                await h.finished("recovery")
                assert_terminal(
                    h, tmp_path, "recovery", TTSAudioEndReason.REQUEST_END
                )
                stop.set()
        finally:
            stop.set()

    asyncio.run(run())


@pytest.mark.parametrize(
    "waiter, value", [("pcm_frames", 1), ("finished", "missing")]
)
def test_fixture_waits_fail_with_deadlines(local_extension, waiter, value):
    async def run():
        async def serve(request, body):
            pytest.fail("No HTTP request should be made")

        async with local_extension(serve) as h:
            with pytest.raises(asyncio.TimeoutError):
                await getattr(h, waiter)(value)
            assert not h.requests

    asyncio.run(run())


def test_long_many_chunks_interleaved_cancel_then_recovery(
    local_extension, tmp_path
):
    async def run():
        release = asyncio.Event()
        long_text = "Long deterministic input. " * 1000

        async def serve(request, body):
            if body["text"] == long_text:
                response = web.StreamResponse(
                    headers={"Content-Type": "audio/wav"}
                )
                await response.prepare(request)
                await response.write(WAV_HEADER + PCM * 20)
                try:
                    await asyncio.wait_for(release.wait(), 2)
                except asyncio.TimeoutError:
                    pass
                try:
                    await response.write(PCM)
                except ConnectionResetError:
                    pass
                return response
            return web.Response(body=WAV_HEADER + PCM, content_type="audio/wav")

        try:
            async with local_extension(serve) as h:
                await h.input("cancelled", long_text, final=False)
                await h.pcm_frames(100)
                await h.input("queued", "queued first", final=False)
                await h.input("cancelled", "cancelled tail")
                await h.input("queued", "queued tail")
                await h.send("tts_flush", {"flush_id": "cancel-load"})
                assert_terminal(
                    h, tmp_path, "cancelled", TTSAudioEndReason.INTERRUPTED
                )
                assert not any(
                    body.get("request_id") == "queued" for _, body in h.events
                )
                assert not h.ext.recorder_map
                count = len(h.events)
                release.set()
                for index in range(5):
                    request_id = f"recovery-{index}"
                    await h.input(request_id, "recovery")
                    await h.finished(request_id)
                    assert_terminal(
                        h, tmp_path, request_id, TTSAudioEndReason.REQUEST_END
                    )
                assert all(
                    body.get("request_id") not in ("cancelled", "queued")
                    for _, body in h.events[count:]
                )
                assert h.requests[0]["text"] == long_text
                assert [body["text"] for body in h.requests[1:]] == [
                    "recovery"
                ] * 5
        finally:
            release.set()

    asyncio.run(asyncio.wait_for(run(), 5))


@pytest.mark.parametrize(
    "output",
    [
        {"audio_pitch": -4, "audio_tempo": 0.8, "remove_silence_ms": 0},
        {"audio_pitch": 12, "audio_tempo": 2.0, "target_lufs": -18.0},
    ],
)
def test_outgoing_mapping_and_unsupported_controls(local_extension, output):
    async def run():
        async def serve(request, body):
            return web.Response(body=WAV_HEADER + PCM, content_type="audio/wav")

        params = {
            "language": "kor",
            "output": {**output, "audio_format": "mp3"},
            "unsupported_control": "ignored",
            "seed": 42,
        }
        async with local_extension(serve, params) as h:
            result = [item async for item in h.ext.client.get(TEXT, "mapping")]
            assert result[-1][1] == TTS2HttpResponseEventType.END
            body = h.requests[0]
            assert body["voice_id"] == "fixture-voice"
            assert body["model"] == "ssfm-v31"
            assert body["language"] == "kor"
            assert body["text"] == TEXT
            assert all(
                body["output"][key] == value for key, value in output.items()
            )
            assert body["output"]["audio_format"] == "wav"
            assert (
                not {
                    "api_key",
                    "url",
                    "dump",
                    "chunk_size",
                    "seed",
                    "unsupported_control",
                }
                & body.keys()
            )

    asyncio.run(run())


@pytest.mark.parametrize(
    "output",
    [
        {"audio_pitch": 13},
        {"audio_pitch": -13},
        {"audio_pitch": 1.5},
        {"audio_pitch": True},
        {"audio_tempo": False},
        {"target_lufs": True},
        {"audio_tempo": 0.49},
        {"audio_tempo": 2.01},
        {"target_lufs": -71},
        {"volume": 100},
        {"sample_rate": 24000},
        *[
            {field: value}
            for field in ("audio_pitch", "audio_tempo", "target_lufs")
            for value in (float("nan"), float("inf"), float("-inf"))
        ],
    ],
)
def test_invalid_output_is_rejected_without_sensitive_input(output):
    config = TypecastTTSConfig(
        params={"api_key": KEY, "voice_id": "voice", "output": output}
    )
    config.update_params()
    with pytest.raises(ValueError, match="Invalid Typecast output settings"):
        config.validate()


@pytest.mark.parametrize(
    "chunk_size", [True, False, 0, -1, 1.0, 1.5, "128", None]
)
def test_chunk_size_preserves_sdk_integer_contract(chunk_size):
    with pytest.raises(ValueError, match="chunk_size"):
        TypecastTTSConfig(chunk_size=chunk_size)


def test_ttfb_excludes_setup_decode_and_event_delivery(local_extension):
    async def run():
        ticks = [0]
        latencies = iter((25, 40))

        async def serve(request, body):
            ticks[0] += next(latencies) * 1_000_000
            return web.Response(body=WAV_HEADER + PCM, content_type="audio/wav")

        async with local_extension(serve) as h:
            ensure_client = h.ext.client._ensure_client
            audio_start = h.ext.send_tts_audio_start
            feed = StreamingWavToPcm16.feed

            def decode(converter, chunk):
                ticks[0] += 50_000_000
                return feed(converter, chunk)

            async def prepare():
                if h.ext.client._client is None:
                    ticks[0] += 100_000_000
                return await ensure_client()

            async def deliver_start(*args, **kwargs):
                ticks[0] += 250_000_000
                await audio_start(*args, **kwargs)

            with patch.object(
                h.ext.client, "_ensure_client", side_effect=prepare
            ), patch.object(
                h.ext, "send_tts_audio_start", side_effect=deliver_start
            ), patch.object(
                StreamingWavToPcm16, "feed", decode
            ), patch(
                "typecast_tts_python.typecast_tts.monotonic_ns",
                side_effect=lambda: ticks[0],
            ), patch(
                "ten_ai_base.tts2_http.datetime"
            ) as clock:
                clock.now.side_effect = lambda: datetime(
                    2026, 1, 1
                ) + timedelta(microseconds=ticks[0] // 1000)
                for turn_id, request_id in enumerate(("first", "second"), 1):
                    await h.send(
                        "tts_text_input",
                        {
                            "request_id": request_id,
                            "text": TEXT,
                            "text_input_end": True,
                            "metadata": {"turn_id": turn_id},
                        },
                    )
                    await h.finished(request_id)
            ttfb = [m for m in h.metrics if "ttfb" in m["metrics"]]
            assert [m["metrics"]["ttfb"] for m in ttfb] == [25, 40]
            assert [m["metadata"]["turn_id"] for m in ttfb] == [1, 2]

    asyncio.run(run())


@pytest.mark.parametrize("output", [[], [1], "", TEXT, 0, False, 1.5])
@pytest.mark.parametrize("method", ["update_params", "validate"])
def test_non_object_output_is_rejected(output, method):
    config = TypecastTTSConfig(
        params={
            "api_key": KEY,
            "voice_id": "voice",
            "model": "ssfm-v31",
            "output": output,
        }
    )
    with pytest.raises(ValueError, match="Invalid Typecast output settings"):
        getattr(config, method)()


def test_config_validation_and_sdk_error_diagnostics_redact(local_extension):
    async def run():
        async def serve(request, body):
            return web.Response(status=429, text=f"{KEY} {TEXT}")

        async with local_extension(
            serve, {"prompt": {"emotion_type": "smart", "previous_text": TEXT}}
        ) as h:
            for safe in (
                h.ext.config.to_str(),
                h.ext.config.to_str(False),
                str(h.ext.config),
                repr(h.ext.config),
            ):
                assert KEY not in safe and TEXT not in safe
            with pytest.raises(
                ValueError, match="Invalid Typecast configuration"
            ) as error:
                await h.ext.create_config(
                    json.dumps({"chunk_size": TEXT, "params": {"api_key": KEY}})
                )
            assert KEY not in str(error.value) and TEXT not in str(error.value)
            await h.input("private-error", TEXT)
            await h.finished("private-error")
            errors = [body for name, body in h.events if name == "error"]
            assert len(errors) == 1
            assert "HTTP 429" in errors[0]["message"]
            assert KEY not in json.dumps(errors) and TEXT not in json.dumps(
                errors
            )
            # Shared TEN transcript logs are separate from adapter diagnostics.
            own_logs = [
                str(call)
                for method in (h.env.log_error, h.env.log_warn)
                for call in method.call_args_list
            ]
            assert all(
                KEY not in line and TEXT not in line for line in own_logs
            )
            h.ext.config.params["model"] = TEXT
            result = [
                item async for item in h.ext.client.get(TEXT, "invalid-model")
            ]
            assert result[-1][1] == TTS2HttpResponseEventType.ERROR
            assert KEY.encode() not in result[-1][0]
            assert TEXT.encode() not in result[-1][0]
            assert len(h.requests) == 1

    asyncio.run(run())
