import asyncio
from dataclasses import dataclass
import aiohttp
import json
from datetime import datetime
from typing import AsyncIterator

from ten.async_ten_env import AsyncTenEnv
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
            "api_key": "",
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
        ttfb = None

        # with open('/app/agents/ten_packages/extension/minimax_tts_python/output.pcm', 'rb') as wf:
        #     audio_data = wf.read()
        #     yield audio_data

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
            except Exception as e:
                ten_env.log_error(f"Error during TTS request: {e}")

        return

        ten_env.log_info(f"headers: {headers}")
        async with aiohttp.ClientSession() as session:
            try:
                async with session.post(
                    url, headers=headers, data=payload
                ) as response:
                    trace_id = ""
                    alb_receive_time = ""

                    try:
                        trace_id = response.headers.get("Trace-Id")
                    except Exception:
                        ten_env.log_warn("get response, no Trace-Id")
                    try:
                        alb_receive_time = response.headers.get(
                            "alb_receive_time"
                        )
                    except Exception:
                        ten_env.log_warn("get response, no alb_receive_time")

                    ten_env.log_info(
                        f"get response trace-id: {trace_id}, alb_receive_time: {alb_receive_time}, cost_time {self._duration_in_ms_since(start_time)}ms"
                    )

                    if response.status != 200:
                        raise RuntimeError(
                            f"Request failed with status {response.status}"
                        )

                    buffer = b""
                    async for chunk in response.content.iter_chunked(
                        1024
                    ):  # Read in 1024 byte chunks
                        buffer += chunk

                        # Split the buffer into lines based on newline character
                        while b"\n" in buffer:
                            line, buffer = buffer.split(b"\n", 1)

                            # Process only lines that start with "data:"
                            if line.startswith(b"data:"):
                                try:
                                    json_data = json.loads(
                                        line[5:].decode("utf-8").strip()
                                    )

                                    # Check for the required keys in the JSON data
                                    if (
                                        "data" in json_data
                                        and "extra_info" not in json_data
                                    ):
                                        audio = json_data["data"].get("audio")
                                        if audio:
                                            decoded_hex = bytes.fromhex(audio)
                                            yield decoded_hex
                                except (
                                    json.JSONDecodeError,
                                    UnicodeDecodeError,
                                ) as e:
                                    # Handle malformed JSON or decoding errors
                                    ten_env.log_warn(
                                        f"Error decoding line: {e}"
                                    )
                                    continue
                        if not ttfb:
                            ttfb = self._duration_in_ms_since(start_time)
                            ten_env.log_info(
                                f"trace-id: {trace_id}, ttfb {ttfb}ms"
                            )
            except aiohttp.ClientError as e:
                ten_env.log_error(f"Client error occurred: {e}")
            except asyncio.TimeoutError:
                ten_env.log_error("Request timed out")
            finally:
                ten_env.log_info(
                    f"http loop done, cost_time {self._duration_in_ms_since(start_time)}ms"
                )

    def _duration_in_ms(self, start: datetime, end: datetime) -> int:
        return int((end - start).total_seconds() * 1000)

    def _duration_in_ms_since(self, start: datetime) -> int:
        return self._duration_in_ms(start, datetime.now())
