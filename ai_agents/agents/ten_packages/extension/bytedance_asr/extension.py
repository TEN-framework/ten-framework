import asyncio
from typing import Any, Dict, List
from pydantic import BaseModel
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import ErrorMessage, ModuleType
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
)
from .bytedance_asr import AsrWsClient
from dataclasses import dataclass, field


@dataclass
class BytedanceASRConfig(BaseModel):
    # Refer to: https://www.volcengine.com/docs/6561/80818.
    # agora_rtc subscribe_audio_samples_per_frame needs to be set to 3200 according to https://www.volcengine.com/docs/6561/111522
    appid: str = ""
    token: str = ""
    api_url: str = "wss://openspeech.bytedance.com/api/v2/asr"
    cluster: str = "volcengine_streaming_common"
    params: Dict[str, Any] = field(default_factory=dict)
    black_list_params: List[str] = field(default_factory=lambda: [])

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class BytedanceASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.connected = False
        self.client = None
        self.config: BytedanceASRConfig = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json = cmd.to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)  # Adjust the sleep time as needed

        await self.stop_connection()
        await self.start_connection()

    async def start_connection(self) -> None:
        self.ten_env.log_info("start and listen bytedance_asr")

        if not self.config:
            config_json, _ = await self.ten_env.get_property_to_json("")
            self.config = BytedanceASRConfig.model_validate_json(config_json)
            self.ten_env.log_info(f"config: {self.config}")

            if not self.config.appid:
                raise ValueError("appid is required")
            if not self.config.token:
                raise ValueError("token is required")

        async def on_message(result):
            if (
                not result
                or "text" not in result[0]
                or "utterances" not in result[0]
            ):
                self.ten_env.log_warn("Received malformed result.")
                return

            sentence = result[0]["text"]

            if len(sentence) == 0:
                return

            is_final = result[0]["utterances"][0].get(
                "definite", False
            )  # Use get to avoid KeyError
            self.ten_env.log_info(
                f"bytedance_asr got sentence: [{sentence}], is_final: {is_final}"
            )

            transcription = UserTranscription(
                text=sentence,
                is_final=is_final,
                start_ms=0,
                duration_ms=0,  # Duration is not provided in the result
                language="zh-CN",
                metadata={
                    "session_id": self.session_id,
                },
                words=[],
            )

            await self.send_asr_transcription(transcription)

        try:
            self.client = AsrWsClient(
                ten_env=self.ten_env,
                cluster=self.config.cluster,
                appid=self.config.appid,
                token=self.config.token,
                api_url=self.config.api_url,
                handle_received_message=on_message,
            )

            # connect to websocket
            await self.client.start()
            self.connected = True
        except Exception as e:
            self.ten_env.log_error(f"Failed to start Bytedance ASR client: {e}")
            error_message = ErrorMessage(
                code=1,
                message=str(e),
                turn_id=0,
                module=ModuleType.STT,
            )
            await self.send_asr_error(error_message, None)
            if not self.stopped:
                # If the extension is not stopped, attempt to reconnect
                await self._handle_reconnect()

    async def stop_connection(self) -> None:
        if self.client:
            await self.client.finish()
            self.client = None
            self.connected = False

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        if self.client:
            await self.client.send(frame.get_buf())

    async def finalize(self, session_id: str | None) -> None:
        raise NotImplementedError(
            "Bytedance ASR does not support finalize operation yet."
        )

    def is_connected(self) -> bool:
        return self.connected

    def input_audio_sample_rate(self) -> int:
        return 16000
