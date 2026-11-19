#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""GMI Cloud request-queue client for text-to-speech.

One client covers the speech model families GMI Cloud hosts behind its
request-queue API. The request shape is selected from the model id:

- ``inworld-tts-*`` (low latency; voice as ``voice_id``)
- ``minimax-tts-speech-*`` (voice as ``voice_id``)
- ``Chatterbox-tts`` (requires a Resemble ``voice_uuid`` as the voice)

Responses may arrive synchronously (with an audio URL inline) or queued
(with a ``request_id`` to poll). Audio is resolved to WAV, then normalized
to mono 16-bit PCM at the configured sample rate before it is emitted as
``TTS2HttpResponseEventType.RESPONSE`` events.
"""
from __future__ import annotations

import asyncio
import io
import re
import struct
import wave
from typing import Any, AsyncIterator, Tuple

import httpx
import numpy as np
from ten_ai_base.struct import TTS2HttpResponseEventType
from ten_ai_base.tts2_http import AsyncTTS2HttpClient

QUEUE_PATH = "/api/v1/ie/requestqueue/apikey/requests"

FAMILY_DEFAULTS: dict[str, dict[str, Any]] = {
    "inworld": {"voice": "Dennis"},
    "minimax": {"voice": "English_expressive_narrator"},
    "chatterbox": {"voice": None},
}

# Sentence boundary splitter for long-text chunking. GMI caps text per
# request (2,000 characters on Inworld); longer input is synthesized in
# sentence-bounded parts and concatenated as continuous PCM. CJK sentences
# end without trailing whitespace, so their punctuation splits on its own;
# Latin punctuation only splits when followed by whitespace (so numbers
# like 3.14 stay intact).
_SENTENCE_SPLIT = re.compile(r"(?<=[。！？])|(?<=[.!?])\s+")
_CJK_PUNCT = ("。", "！", "？")

# HTTP statuses worth retrying while polling a queued generation.
TRANSIENT_POLL_STATUSES = (408, 425, 429)


class GmicloudTTSApiError(Exception):
    """HTTP or generation failure with context for error mapping."""

    def __init__(
        self,
        message: str,
        *,
        status_code: int | None = None,
        request_id: str = "",
    ):
        super().__init__(message)
        self.status_code = status_code
        self.request_id = request_id


def model_family(model: str) -> str:
    """Map a GMI Cloud model id to its request family."""
    m = (model or "").lower()
    if "chatterbox" in m:
        return "chatterbox"
    if "minimax" in m:
        return "minimax"
    return "inworld"


def build_payload(
    text: str,
    *,
    model: str,
    voice: str | None,
    audio_format: str,
    speed: float,
) -> dict[str, Any]:
    """Build the GMI Cloud request payload for the model's request family.

    Raises:
        ValueError: Chatterbox models without a Resemble ``voice_uuid``.
    """
    family = model_family(model)
    defaults = FAMILY_DEFAULTS[family]
    resolved_voice = voice if voice is not None else defaults["voice"]
    encoding = (audio_format or "WAV").lower()

    if family == "inworld":
        payload: dict[str, Any] = {
            "text": text,
            "audio_encoding": encoding.upper(),
            "speaking_rate": speed,
        }
        if resolved_voice:
            payload["voice_id"] = resolved_voice
    elif family == "minimax":
        payload = {
            "text": text,
            "speed": str(speed),
            "format": "wav" if encoding == "wav" else encoding,
        }
        if resolved_voice:
            payload["voice_id"] = resolved_voice
    else:  # chatterbox
        if not resolved_voice:
            raise ValueError(
                "Chatterbox models require a Resemble voice UUID (voice_uuid): "
                "pass voice='<your-voice-uuid>' in params "
                "(or use a different model)."
            )
        payload = {
            "data": text,
            "voice_uuid": resolved_voice,
            "output_format": "wav" if encoding == "wav" else encoding,
        }
    return payload


def extract_audio_url(data: dict[str, Any]) -> str | None:
    """Extract the first audio URL from a queue response outcome."""
    outcome = data.get("outcome") or {}
    if not isinstance(outcome, dict):
        return None
    audio_url = outcome.get("audio_url")
    if audio_url:
        return audio_url
    for key in ("media_urls", "medias", "media"):
        for item in outcome.get(key) or []:
            if isinstance(item, dict) and item.get("url"):
                return item["url"]
    return None


def parse_wav(data: bytes) -> tuple[bytes, int, int, int] | None:
    """Decode a WAV blob into (pcm, sample_rate, channels, sample_width)."""
    try:
        with wave.open(io.BytesIO(data), "rb") as wav:
            pcm = wav.readframes(wav.getnframes())
            return pcm, wav.getframerate(), wav.getnchannels(), wav.getsampwidth()
    except Exception:  # noqa: BLE001 - any unparsable blob is simply not a WAV
        return None


def to_mono_s16(pcm: bytes, channels: int) -> bytes:
    """Downmix interleaved 16-bit PCM to mono by averaging the channels."""
    if channels <= 1:
        return pcm
    samples = struct.unpack(f"<{len(pcm) // 2}h", pcm)
    mono = [
        sum(samples[i : i + channels]) // channels
        for i in range(0, len(samples), channels)
    ]
    return struct.pack(f"<{len(mono)}h", *mono)


def resample_s16(pcm: bytes, src_rate: int, dst_rate: int) -> bytes:
    """Resample mono 16-bit PCM to another rate (linear interpolation)."""
    if src_rate == dst_rate:
        return pcm
    samples = np.frombuffer(pcm, dtype=np.int16).astype(np.float32)
    if samples.size == 0:
        return pcm
    n_out = max(1, round(samples.size * dst_rate / src_rate))
    x_old = np.linspace(0.0, 1.0, num=samples.size, endpoint=False)
    x_new = np.linspace(0.0, 1.0, num=n_out, endpoint=False)
    out = np.interp(x_new, x_old, samples)
    return np.clip(out, -32768, 32767).astype(np.int16).tobytes()


def split_text(text: str, chunk_chars: int) -> list[str]:
    """Split text at sentence boundaries into chunks of at most chunk_chars."""
    text = text.strip()
    if not text:
        return []
    if chunk_chars <= 0 or len(text) <= chunk_chars:
        return [text]
    parts: list[str] = []
    current = ""
    for sentence in _SENTENCE_SPLIT.split(text):
        piece = sentence.strip()
        if not piece:
            continue
        if current and len(current) + 1 + len(piece) > chunk_chars:
            parts.append(current)
            current = piece
        else:
            joiner = "" if current.endswith(_CJK_PUNCT) else " "
            current = f"{current}{joiner}{piece}".strip()
        while len(current) > chunk_chars:
            parts.append(current[:chunk_chars])
            current = current[chunk_chars:]
    if current:
        parts.append(current)
    return parts


class GmicloudTTSOptions:
    def __init__(
        self,
        api_key: str,
        base_url: str,
        model: str,
        voice: str | None,
        audio_format: str,
        speed: float,
        sample_rate: int,
        text_chunk_chars: int,
        request_timeout_sec: float,
        poll_interval_sec: float,
        poll_attempts: int,
    ):
        self.api_key = api_key
        self.base_url = base_url
        self.model = model
        self.voice = voice
        self.audio_format = audio_format
        self.speed = speed
        self.sample_rate = sample_rate
        self.text_chunk_chars = text_chunk_chars
        self.request_timeout_sec = request_timeout_sec
        self.poll_interval_sec = poll_interval_sec
        self.poll_attempts = poll_attempts


class GmicloudTTSClient(AsyncTTS2HttpClient):
    """Async GMI Cloud TTS client emitting normalized PCM frames.

    ``get`` maps the request-queue protocol onto the TTS2 HTTP event
    contract: audio bytes as ``RESPONSE``, clean completion as ``END``,
    auth failures as ``INVALID_KEY_ERROR`` and everything else as
    ``ERROR``. The extension base owns the error envelope and the request
    lifecycle around these events.
    """

    def __init__(self, opts: GmicloudTTSOptions):
        self.opts = opts
        self._client = httpx.AsyncClient(timeout=opts.request_timeout_sec)
        self._cancelled = False

    async def clean(self) -> None:
        await self._client.aclose()

    async def cancel(self) -> None:
        self._cancelled = True

    def get_extra_metadata(self) -> dict[str, Any]:
        return {"model": self.opts.model, "voice": self.opts.voice or ""}

    async def get(
        self, text: str, request_id: str
    ) -> AsyncIterator[Tuple[bytes | None, TTS2HttpResponseEventType]]:
        try:
            for part in split_text(text, self.opts.text_chunk_chars):
                if self._cancelled:
                    yield None, TTS2HttpResponseEventType.FLUSH
                    return
                audio = await self._synthesize_part(part, request_id)
                pcm = self._normalize(audio)
                frame_bytes = max(2, self.opts.sample_rate // 50 * 2)
                for offset in range(0, len(pcm), frame_bytes):
                    if self._cancelled:
                        yield None, TTS2HttpResponseEventType.FLUSH
                        return
                    yield (
                        pcm[offset : offset + frame_bytes],
                        TTS2HttpResponseEventType.RESPONSE,
                    )
            yield None, TTS2HttpResponseEventType.END
        except GmicloudTTSApiError as error:
            if self._cancelled:
                yield None, TTS2HttpResponseEventType.FLUSH
                return
            message = str(error).encode()
            if error.status_code in (401, 403):
                yield message, TTS2HttpResponseEventType.INVALID_KEY_ERROR
            else:
                yield message, TTS2HttpResponseEventType.ERROR
        except asyncio.CancelledError:
            raise
        except Exception as error:  # noqa: BLE001 - never crash the pipeline
            yield str(error).encode(), TTS2HttpResponseEventType.ERROR

    def _normalize(self, audio: bytes) -> bytes:
        parsed = parse_wav(audio)
        if parsed is None:
            raise GmicloudTTSApiError(
                "GMI Cloud audio was not a parseable WAV file"
            )
        pcm, rate, channels, width = parsed
        if width != 2:
            raise GmicloudTTSApiError(
                f"unsupported sample width: {width * 8}-bit"
            )
        pcm = to_mono_s16(pcm, channels)
        return resample_s16(pcm, rate, self.opts.sample_rate)

    async def _synthesize_part(self, text: str, request_id: str) -> bytes:
        body = {
            "model": self.opts.model,
            "payload": build_payload(
                text,
                model=self.opts.model,
                voice=self.opts.voice,
                audio_format=self.opts.audio_format,
                speed=self.opts.speed,
            ),
        }
        headers = {
            "Authorization": f"Bearer {self.opts.api_key}",
            "Content-Type": "application/json",
        }
        try:
            response = await self._client.post(
                f"{self.opts.base_url}{QUEUE_PATH}",
                json=body,
                headers=headers,
            )
        except httpx.HTTPError as error:
            raise GmicloudTTSApiError(
                f"GMI Cloud request error: {error}"
            ) from error
        if response.status_code != 200:
            raise GmicloudTTSApiError(
                f"GMI Cloud API error: HTTP {response.status_code} - "
                f"{response.text[:500]}",
                status_code=response.status_code,
            )
        data = response.json()
        data, audio_url = await self._resolve_result(data)
        if data.get("status") != "success" or not audio_url:
            raise GmicloudTTSApiError(
                f"GMI Cloud TTS failed: status={data.get('status')!r}, "
                f"request_id={data.get('request_id')!r}",
                request_id=str(data.get("request_id") or ""),
            )
        try:
            audio = await self._client.get(audio_url)
        except httpx.HTTPError as error:
            raise GmicloudTTSApiError(
                f"GMI Cloud audio download error: {error}"
            ) from error
        if audio.status_code != 200:
            raise GmicloudTTSApiError(
                f"GMI Cloud audio download error: HTTP {audio.status_code} - "
                f"{audio.text[:200]}",
                status_code=audio.status_code,
            )
        return audio.content

    async def _resolve_result(
        self, data: dict[str, Any]
    ) -> tuple[dict[str, Any], str | None]:
        """Resolve a response that may be synchronous or queued."""
        audio_url = extract_audio_url(data)
        status = data.get("status")
        request_id = data.get("request_id")

        if not audio_url and status != "failed" and request_id:
            poll_url = f"{self.opts.base_url}{QUEUE_PATH}/{request_id}"
            for _ in range(self.opts.poll_attempts):
                await asyncio.sleep(self.opts.poll_interval_sec)
                if self._cancelled:
                    raise GmicloudTTSApiError("cancelled during queue poll")
                try:
                    response = await self._client.get(
                        poll_url,
                        headers={
                            "Authorization": f"Bearer {self.opts.api_key}"
                        },
                    )
                except httpx.HTTPError as error:
                    raise GmicloudTTSApiError(
                        f"GMI Cloud queue poll error: {error}",
                        request_id=str(request_id),
                    ) from error
                if response.status_code == 200:
                    data = response.json()
                elif (
                    response.status_code in TRANSIENT_POLL_STATUSES
                    or response.status_code >= 500
                ):
                    continue
                else:
                    raise GmicloudTTSApiError(
                        f"GMI Cloud queue poll error: HTTP "
                        f"{response.status_code} - {response.text[:500]}",
                        status_code=response.status_code,
                        request_id=str(request_id),
                    )
                audio_url = extract_audio_url(data)
                status = data.get("status")
                if status in ("success", "failed") or audio_url:
                    break
            else:
                raise GmicloudTTSApiError(
                    "GMI Cloud queue poll timed out",
                    request_id=str(request_id),
                )
        return data, audio_url
