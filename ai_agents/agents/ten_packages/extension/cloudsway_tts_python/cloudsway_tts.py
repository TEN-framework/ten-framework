import asyncio
from dataclasses import dataclass
import aiohttp
from datetime import datetime
from typing import AsyncIterator

from ten_runtime.async_ten_env import AsyncTenEnv
from ten_ai_base.config import BaseConfig


@dataclass
class CloudswayTTSConfig(BaseConfig):
    api_key: str = ""
    model: str = "cloudsway_tts"
    voice_id: str = "Luna_normal_1"
    sample_rate: int = 32000
    url: str = "http://0.0.0.0:9880/tts/stream"
    language: str = "en"


class CloudswayTTS:
    def __init__(self, config: CloudswayTTSConfig):
        self.config = config

    async def get(
        self, ten_env: AsyncTenEnv, text: str
    ) -> AsyncIterator[bytes]:
        payload = {
            "api_key": self.config.api_key,
            "model": self.config.model,
            "text": text,
            "voice_id": self.config.voice_id,
            "language": self.config.language,
            "sample_rate": self.config.sample_rate,
        }

        ten_env.log_info(f"payload: {payload}")

        headers = {
            "accept": "application/json",
            # "Authorization": f"Bearer {self.config.api_key}",
            "Content-Type": "application/json",
        }

        start_time = datetime.now()
        ten_env.log_info(f"Start request, url: {self.config.url}, text: {text}")
        # ttfb = None

        async with aiohttp.ClientSession() as session:
            try:
                async with session.post(
                    self.config.url,
                    headers=headers,
                    json=payload,
                ) as response:
                    if response.status != 200:
                        raise RuntimeError(
                            f"Request failed with status {response.status}"
                        )

                    first_chunk = True
                    async for chunk in response.content.iter_any():
                        ten_env.log_info("Received chunk of audio data.")
                        if first_chunk:
                            yield chunk[44:]  # 去除前44个byte
                            first_chunk = False
                        else:
                            yield chunk
            except aiohttp.ClientError as e:
                ten_env.log_error(f"Client error occurred: {e}")
            except asyncio.TimeoutError:
                ten_env.log_error("Request timed out")
            finally:
                ten_env.log_info(
                    f"http loop done, cost_time {self._duration_in_ms_since(start_time)}ms"
                )

        return

    def _duration_in_ms(self, start: datetime, end: datetime) -> int:
        return int((end - start).total_seconds() * 1000)

    def _duration_in_ms_since(self, start: datetime) -> int:
        return self._duration_in_ms(start, datetime.now())
