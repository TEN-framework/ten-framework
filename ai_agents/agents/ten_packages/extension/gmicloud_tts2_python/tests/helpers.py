#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import io
import struct
import wave
from unittest.mock import AsyncMock


def make_wav(pcm: bytes, rate: int, channels: int) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav:
        wav.setnchannels(channels)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        wav.writeframes(pcm)
    return buf.getvalue()


def make_opts(**overrides):
    from gmicloud_tts2_python.client import GmicloudTTSOptions

    base = dict(
        api_key="test-key-value",
        base_url="https://console.gmicloud.ai",
        model="inworld-tts-2",
        voice="Dennis",
        audio_format="WAV",
        speed=1.0,
        sample_rate=24000,
        text_chunk_chars=2000,
        request_timeout_sec=5.0,
        poll_interval_sec=0.01,
        poll_attempts=3,
    )
    base.update(overrides)
    return GmicloudTTSOptions(**base)


class FakeResponse:
    def __init__(self, status_code=200, json_data=None, content=b""):
        self.status_code = status_code
        self._json = json_data or {}
        self.content = content

    def json(self):
        return self._json

    @property
    def text(self):
        import json as _json
        return _json.dumps(self._json) if self._json else self.content.decode("utf-8", "replace")


async def collect(client, text):
    events = []
    async for event in client.get(text, "req-1"):
        events.append(event)
    return events


def split_events(events):
    # Only RESPONSE payloads are audio; error events carry message bytes.
    from ten_ai_base.struct import TTS2HttpResponseEventType

    audio = bytearray()
    kinds = []
    for payload, kind in events:
        kinds.append(kind)
        if payload and kind == TTS2HttpResponseEventType.RESPONSE:
            audio += payload
    return bytes(audio), kinds
