from datetime import datetime
from typing import Any, Dict, List
from pydantic import BaseModel
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ErrorMessage, ErrorMessageVendorInfo, ModuleType
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
    StatusCode,
    CmdResult,
)

import asyncio

from deepgram import (
    DeepgramClientOptions,
    LiveTranscriptionEvents,
    LiveOptions,
)
import deepgram
from dataclasses import dataclass, field


@dataclass
class DeepgramASRConfig(BaseModel):
    api_key: str = ""
    language: str = "en-US"
    model: str = "nova-2"
    sample_rate: int = 16000
    encoding: str = "linear16"
    interim_results: bool = True
    punctuate: bool = True
    params: Dict[str, Any] = field(default_factory=dict)
    black_list_params: List[str] = field(
        default_factory=lambda: [
            "channels",
            "encoding",
            "multichannel",
            "sample_rate",
            "callback_method",
            "callback",
        ]
    )

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class DeepgramASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.connected = False
        self.client: deepgram.AsyncListenWebSocketClient = None
        self.config: DeepgramASRConfig = None
        self.last_finalize_timestamp: int = 0

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("DeepgramASRExtension on_init")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json, _ = cmd.get_property_to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)
        await self.start_connection()

    def _on_close(self, *args, **kwargs):
        self.ten_env.log_info(
            f"deepgram event callback on_close: {args}, {kwargs}"
        )
        self.connected = False
        if not self.stopped:
            self.ten_env.log_warn(
                "Deepgram connection closed unexpectedly. Reconnecting..."
            )
            asyncio.create_task(self._handle_reconnect())

    async def _on_open(self, _, event):
        self.ten_env.log_info(f"deepgram event callback on_open: {event}")
        self.connected = True

    async def _on_error(self, _, error):
        self.ten_env.log_error(
            f"deepgram event callback on_error: {error.to_json()}"
        )

        if self.on_error:
            error_message = ErrorMessage(
                code=-1,
                message=error.to_json(),
                turn_id=0,
                module=ModuleType.STT,
            )

            await self.send_asr_error(
                error_message,
                ErrorMessageVendorInfo(
                    vendor="deepgram",
                    code=error.code,
                    message=error.message,
                ),
            )

    async def _on_message(self, _, result):
        try:
            sentence = result.channel.alternatives[0].transcript

            if not sentence:
                return

            start_ms = int(
                result.start * 1000
            )  # convert seconds to milliseconds
            duration_ms = int(
                result.duration * 1000
            )  # convert seconds to milliseconds

            is_final = result.is_final
            final_from_finalize = is_final and result.from_finalize
            await self._finalize_counter_if_needed(final_from_finalize)
            self.ten_env.log_info(
                f"deepgram got sentence: [{sentence}], is_final: {is_final}"
            )

            transcription = UserTranscription(
                text=sentence,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
                words=[],
            )
            await self.send_asr_transcription(transcription)
        except Exception as e:
            self.ten_env.log_error(f"Error processing message: {e}")
            await self.send_asr_error(
                ErrorMessage(
                    code=1,
                    message=str(e),
                    turn_id=0,
                    module=ModuleType.STT,
                ),
                None,
            )

    async def start_connection(self) -> None:
        self.ten_env.log_info("start and listen deepgram")

        if self.config is None:
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = DeepgramASRConfig.model_validate_json(config_json)
            self.ten_env.log_debug(f"config: {self.config}")

            if not self.config.api_key:
                self.ten_env.log_error("get property api_key")
                return

        await self.stop_connection()

        self.client = deepgram.AsyncListenWebSocketClient(
            config=DeepgramClientOptions(
                api_key=self.config.api_key, options={"keepalive": "true"}
            )
        )

        self.client.on(LiveTranscriptionEvents.Open, self._on_open)
        self.client.on(LiveTranscriptionEvents.Close, self._on_close)
        self.client.on(LiveTranscriptionEvents.Transcript, self._on_message)
        self.client.on(LiveTranscriptionEvents.Error, self._on_error)

        options = LiveOptions(
            language=self.config.language,
            model=self.config.model,
            sample_rate=self.input_audio_sample_rate(),
            channels=self.input_audio_channels(),
            encoding=self.config.encoding,
            interim_results=self.config.interim_results,
            punctuate=self.config.punctuate,
        )

        # Update options with params
        if self.config.params:
            for key, value in self.config.params.items():
                # Check if it's a valid option and not in black list
                if hasattr(
                    options, key
                ) and not self.config.is_black_list_params(key):
                    setattr(self.options, key, value)

        self.ten_env.log_info(f"deepgram options: {options}")
        # connect to websocket
        result = await self.client.start(options)
        if not result:
            self.ten_env.log_error("failed to connect to deepgram")
            await self._handle_reconnect()
        else:
            self.ten_env.log_info("successfully connected to deepgram")

    async def stop_connection(self) -> None:
        try:
            if self.client:
                await self.client.finish()
                self.client = None
                self.connected = False
                self.ten_env.log_info("deepgram connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping deepgram connection: {e}")

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> None:
        frame_buf = frame.get_buf()
        return await self.client.send(frame_buf)

    def is_connected(self) -> bool:
        return self.connected and self.client is not None

    async def finalize(self, session_id: str | None) -> None:
        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_debug(
            f"deepgram drain start at {self.last_finalize_timestamp} session_id: {session_id}"
        )
        await self.client.finalize()

    async def _finalize_counter_if_needed(self, is_final: bool) -> None:
        if is_final and self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT deepgram drain end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end(latency)

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate
