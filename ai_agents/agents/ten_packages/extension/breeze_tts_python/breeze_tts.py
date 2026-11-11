"""Breeze TTS client for the TEN Framework extension.

Wraps Breeze's streaming TTS endpoint and yields raw PCM chunks that the
TEN audio pipeline can forward as-is.
"""

from __future__ import annotations

import os
from typing import AsyncIterator

import aiohttp
from pydantic import BaseModel, Field


class BreezeTTSParams(BaseModel):
    """Connection and synthesis parameters."""

    api_key: str = Field(
        default_factory=lambda: os.getenv("BREEZE_API_KEY", ""),
        description="Breeze API key (brz_...)",
    )
    base_url: str = Field(
        default_factory=lambda: os.getenv("BREEZE_BASE_URL", "https://api.breeze.blue"),
        description="Breeze API base URL",
    )
    voice_id: str = Field(default="voc_u3nxkd5578wc", description="Breeze voice id")
    language: str = Field(default="zh", description="ISO 639-1 language code")
    speed: float = Field(default=1.0, description="Speaking rate, 0.5 - 2.0")
    instructions: str = Field(default="", description="Optional style instructions")
    sample_rate: int = Field(default=24000, description="PCM sample rate in Hz")
    output_format: str = Field(
        default="pcm", description="Breeze output format query value"
    )
    timeout: int = Field(default=60, description="HTTP timeout in seconds")


class BreezeTTS:
    """Small async client around the Breeze streaming endpoint."""

    def __init__(self, params: BreezeTTSParams) -> None:
        self.params = params
        self.session: aiohttp.ClientSession | None = None

    async def start(self) -> None:
        if self.session is None or self.session.closed:
            self.session = aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=self.params.timeout)
            )

    async def stop(self) -> None:
        if self.session is not None and not self.session.closed:
            await self.session.close()
        self.session = None

    async def synthesize(self, text: str) -> AsyncIterator[bytes]:
        await self.start()
        assert self.session is not None

        url = f"{self.params.base_url.rstrip('/')}/v1/text-to-speech/{self.params.voice_id}/stream"
        body: dict = {"text": text, "language_code": self.params.language}
        if self.params.instructions:
            body["instructions"] = self.params.instructions[:500]
        if self.params.speed != 1.0:
            body["voice_settings"] = {"speed": self.params.speed}

        async with self.session.post(
            url,
            params={"output_format": self.params.output_format},
            json=body,
            headers={"xi-api-key": self.params.api_key},
        ) as response:
            if response.status != 200:
                detail = (await response.text())[:200]
                raise RuntimeError(f"Breeze {response.status}: {detail}")
            async for chunk in response.content.iter_chunked(4096):
                if chunk:
                    yield chunk
