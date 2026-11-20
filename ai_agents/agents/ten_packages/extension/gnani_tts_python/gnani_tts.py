import asyncio
import base64
import json
import time

import httpx
from ten_ai_base.struct import TTS2HttpResponseEventType as Event
from ten_ai_base.tts2_http import AsyncTTS2HttpClient


async def events(response):
    name, data = "", []
    async for line in response.aiter_lines():
        if not line:
            if data:
                received_ns = time.monotonic_ns()
                yield received_ns, name, json.loads("\n".join(data))
            name, data = "", []
        elif line.startswith("event:"):
            name = line[6:].strip()
        elif line.startswith("data:"):
            data.append(line[5:].lstrip(" "))


class GnaniTTSClient(AsyncTTS2HttpClient):
    url = "https://api.vachana.ai/api/v1/tts/sse"

    def __init__(self, params):
        self.params = params
        self.http = httpx.AsyncClient(timeout=httpx.Timeout(30, connect=5))
        self.response = None
        self.timing_request_id = None
        self.started_ns = None
        self.first_audio_ms = None

    def get_extra_metadata(self):
        return {"model": self.params.model, "voice": self.params.voice}

    async def cancel(self):
        if self.response is not None:
            await self.response.aclose()

    async def clean(self):
        await self.http.aclose()

    async def open_stream(self, headers, payload, request_id):
        # A rejected request can be retried before any audio is delivered.
        for attempt in range(4):
            request = self.http.build_request(
                "POST", self.url, headers=headers, json=payload
            )
            request.extensions["ten_request_id"] = request_id
            if self.timing_request_id != request_id:
                self.timing_request_id = request_id
                self.started_ns = time.monotonic_ns()
                self.first_audio_ms = None
            response = await self.http.send(request, stream=True)
            if response.is_success:
                return response
            await response.aclose()
            if response.status_code != 429 or attempt == 3:
                response.raise_for_status()
            try:
                delay = float(response.headers.get("Retry-After", 2**attempt))
            except ValueError:
                delay = 2**attempt
            if not 0 <= delay <= 10:
                response.raise_for_status()
            await asyncio.sleep(delay)

    async def get(self, text, request_id):
        if not text.strip():
            yield None, Event.END
            return
        payload = {
            "text": text,
            "model": self.params.model,
            "voice": self.params.voice,
            "language": self.params.language,
            "audio_config": {
                "encoding": "linear_pcm",
                "container": "raw",
                "sample_rate": self.params.sample_rate,
                "num_channels": 1,
                "sample_width": 2,
            },
        }
        headers = {"X-API-Key-ID": self.params.api_key.get_secret_value()}
        tail = b""
        response = None
        terminal = (None, Event.END)
        try:
            response = await self.open_stream(headers, payload, request_id)
            self.response = response
            async for received_ns, name, body in events(response):
                if name == "complete":
                    if tail:
                        raise ValueError("Incomplete PCM sample")
                    break
                if name == "error":
                    raise ValueError("Gnani reported a synthesis error")
                if name == "chunk":
                    chunk = base64.b64decode(body["audio"], validate=True)
                    if chunk and self.first_audio_ms is None:
                        self.first_audio_ms = (
                            received_ns - self.started_ns
                        ) // 1_000_000
                    audio = tail + chunk
                    aligned = len(audio) - len(audio) % 2
                    tail = audio[aligned:]
                    if aligned:
                        yield audio[:aligned], Event.RESPONSE
            else:
                raise ValueError("Stream ended before completion")
        except httpx.HTTPStatusError as exc:
            code = exc.response.status_code
            terminal = f"Gnani HTTP {code}".encode(), (
                Event.INVALID_KEY_ERROR if code in (401, 403) else Event.ERROR
            )
        except (httpx.HTTPError, ValueError, KeyError, TypeError) as exc:
            terminal = (
                f"Gnani stream failed ({type(exc).__name__})".encode(),
                Event.ERROR,
            )
        finally:
            if response is not None:
                await response.aclose()
                if self.response is response:
                    self.response = None
        yield terminal
