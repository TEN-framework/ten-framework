import asyncio
import base64
import json
import requests
import websockets

from dataclasses import dataclass
from datetime import datetime
from typing import Optional

from pydantic import BaseModel
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ErrorMessage, ErrorMessageVendorInfo, ModuleType
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    CmdResult,
    StatusCode,
)


@dataclass
class GladiaASRConfig(BaseModel):
    api_key: str
    language: str = "en-US"
    sample_rate: int = 16000


class GladiaASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[GladiaASRConfig] = None
        self.ws: Optional[websockets.client.ClientProtocol] = None
        self.ws_loop: Optional[asyncio.Task] = None
        self.last_finalize_timestamp: int = 0
        self.connected = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("GladiaASRExtension on_init")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def start_connection(self) -> None:
        self.ten_env.log_info("Starting Gladia Live session...")

        if self.config is None:
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = GladiaASRConfig.model_validate_json(config_json)

        await self.stop_connection()

        try:
            ws_url = self._init_live_session(self.config)
            self.ws = await websockets.connect(ws_url)
        except Exception as e:
            await self._handle_error(e)
            return

        self.connected = True
        self.ws_loop = asyncio.create_task(self._receive_loop())
        self.ten_env.log_info("Gladia WebSocket connected.")

    def _init_live_session(self, config: GladiaASRConfig) -> str:
        payload = {
            "encoding": "wav/pcm",
            "bit_depth": self.input_audio_sample_width() * 8,
            "sample_rate": self.input_audio_sample_rate(),
            "channels": self.input_audio_channels(),
            "language_config": {
                "languages": [],
                "code_switching": True,
            },
        }
        response = requests.post(
            "https://api.gladia.io/v2/live",
            headers={"X-Gladia-Key": config.api_key},
            json=payload,
            timeout=3,
        )
        response.raise_for_status()
        return response.json()["url"]

    async def _receive_loop(self):
        try:
            async for message in self.ws:
                await self._handle_message(message)
        except Exception as e:
            await self._handle_error(e)

    async def _handle_message(self, message: str):
        try:
            data = json.loads(message)
            if data.get("type") != "transcript":
                return

            result = data["data"]
            if not result.get("is_final"):
                return

            utterance = result["utterance"]
            text = utterance.get("text", "").strip()
            start_ms = int(utterance.get("start", 0) * 1000)
            duration_ms = int(
                (utterance.get("end", 0) - utterance.get("start", 0)) * 1000
            )

            final_from_finalize = True
            await self._finalize_counter_if_needed(final_from_finalize)

            transcription = UserTranscription(
                text=text,
                final=True,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
                words=[],
            )
            await self.send_asr_transcription(transcription)
        except Exception as e:
            await self._handle_error(e)

    async def send_audio(
        self, frame: AudioFrame, session_id: Optional[str]
    ) -> None:
        try:
            self.session_id = session_id
            chunk = base64.b64encode(frame.get_buf()).decode("utf-8")
            msg = json.dumps({"type": "audio_chunk", "data": {"chunk": chunk}})
            await self.ws.send(msg)
        except Exception as e:
            await self._handle_error(e)

    async def finalize(self, session_id: Optional[str]) -> None:
        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_info("Sending stop_recording to Gladia...")
        if self.ws:
            try:
                await self.ws.send(json.dumps({"type": "stop_recording"}))
            except Exception as e:
                await self._handle_error(e)

    async def stop_connection(self) -> None:
        if self.ws:
            await self.ws.close()
            self.ws = None
            self.connected = False
            self.ten_env.log_info("Gladia connection closed")

        if self.ws_loop:
            self.ws_loop.cancel()
            self.ws_loop = None

    def is_connected(self) -> bool:
        return self.connected and self.ws is not None

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def _handle_error(self, error: Exception):
        self.ten_env.log_error(f"Gladia error: {error}")
        await self.send_asr_error(
            ErrorMessage(
                code=-1,
                message=str(error),
                turn_id=0,
                module=ModuleType.ASR,
            ),
            ErrorMessageVendorInfo(
                vendor="gladia", code=-1, message=str(error)
            ),
        )

    async def _finalize_counter_if_needed(self, is_final: bool) -> None:
        if is_final and self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT gladia drain end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0

            await self.send_asr_finalize_end()
