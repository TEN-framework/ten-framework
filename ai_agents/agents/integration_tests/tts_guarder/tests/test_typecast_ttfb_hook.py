"""Independent SDK WAV timing and stream ownership regression checks."""

import asyncio
from io import BytesIO
from types import SimpleNamespace
import wave

import pytest

from .provider_ttfb_hooks import install_hook


def wav_header():
    output = BytesIO()
    with wave.Wave_write(output) as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(32000)
        audio.writeframes(b"\x01\x02")
    header = bytearray(output.getvalue()[:-2])
    header[4:8] = header[40:44] = b"\xff" * 4
    return bytes(header)


def test_typecast_raw_sdk_timing_and_request_isolation(monkeypatch):
    now = [0]
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.reference.time.monotonic_ns", lambda: now[0]
    )

    class Client:
        async def text_to_speech_stream(self, request, chunk_size=8192):
            assert chunk_size == 64
            yield b""
            now[0] = 10_000_000
            yield wav_header()[:13]
            await asyncio.sleep(0)
            now[0] = 20_000_000
            yield wav_header()[13:]
            now[0] = 30_000_000
            yield b"\x01"
            now[0] = 40_000_000
            yield b"\x02"
            now[0] = 50_000_000
            yield b"\x03\x04"

    original = Client.text_to_speech_stream
    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.typecast.import_module",
        lambda name: SimpleNamespace(AsyncTypecast=Client),
    )

    async def run():
        with install_hook("typecast_tts_python", {}, monkeypatch) as reference:
            reference.begin_request("first")
            first = Client().text_to_speech_stream(
                SimpleNamespace(text="Hello"), chunk_size=64
            )
            assert await anext(first) == b""
            reference.begin_request("second")
            second = Client().text_to_speech_stream(
                SimpleNamespace(text="Next"), chunk_size=64
            )
            assert await anext(second) == b""
            one, two = await asyncio.gather(collect(first), collect(second))
            assert (
                one
                == two
                == [
                    wav_header()[:13],
                    wav_header()[13:],
                    b"\x01",
                    b"\x02",
                    b"\x03\x04",
                ]
            )
            assert reference.sent == {} and reference.audio == {}
            assert reference.elapsed_ms("first") == 40
            assert reference.elapsed_ms("second") == 40

            now[0] = 100_000_000
            reference.begin_request("empty")
            await collect(
                Client().text_to_speech_stream(
                    SimpleNamespace(text="  "), chunk_size=64
                )
            )
            assert reference.elapsed_ms("empty") is None

            closed = []

            async def cancellable(instance, request, **kwargs):
                try:
                    yield wav_header()
                    await asyncio.sleep(10)
                finally:
                    closed.append(True)

            # Reinstall on a stream with an observable resource lifetime.
            monkeypatch.setattr(Client, "text_to_speech_stream", cancellable)
            with install_hook("typecast_tts_python", {}, monkeypatch):
                stream = Client().text_to_speech_stream(
                    SimpleNamespace(text="cancel")
                )
                assert await anext(stream) == wav_header()
                await stream.aclose()
                assert closed == [True]
            monkeypatch.setattr(Client, "text_to_speech_stream", original)
        assert Client.text_to_speech_stream is original

    asyncio.run(run())


async def collect(stream):
    return [chunk async for chunk in stream]


@pytest.mark.parametrize(
    "chunks", [[b"not WAV"], [wav_header()], [wav_header(), b"\x01"]]
)
def test_typecast_hook_requires_a_complete_pcm_sample(monkeypatch, chunks):
    class Client:
        async def text_to_speech_stream(self, request):
            for chunk in chunks:
                yield chunk

    monkeypatch.setattr(
        "tests.provider_ttfb_hooks.typecast.import_module",
        lambda name: SimpleNamespace(AsyncTypecast=Client),
    )

    async def run():
        with install_hook("typecast_tts_python", {}, monkeypatch) as reference:
            reference.begin_request("first")
            assert (
                await collect(
                    Client().text_to_speech_stream(
                        SimpleNamespace(text="Hello")
                    )
                )
                == chunks
            )
            assert reference.elapsed_ms("first") is None

    asyncio.run(run())
