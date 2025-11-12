from typing import Any, Dict, List
from typing_extensions import override
from pydantic import BaseModel
from ten_ai_base.asr import ASRResult, AsyncASRBaseExtension
import nls
import nls.token
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)

import asyncio

import json

from dataclasses import dataclass, field


@dataclass
class AliyunASRConfig(BaseModel):
    # Refer to: https://help.aliyun.com/zh/isi/developer-reference/sdk-for-python-2.
    appkey: str = ""
    akid: str = ""
    aksecret: str = ""
    api_url: str = "wss://nls-gateway.aliyuncs.com/ws/v1"
    params: Dict[str, Any] = field(default_factory=dict)
    black_list_params: List[str] = field(default_factory=lambda: [])

    def is_black_list_params(self, key: str) -> bool:
        return key in self.black_list_params


class AliyunASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.connected = False
        self.client = None
        self.config: AliyunASRConfig | None = None
        self.loop: asyncio.AbstractEventLoop | None = None

    def vendor(self) -> str:
        return "aliyun"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        self.loop = asyncio.get_event_loop()

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = AliyunASRConfig.model_validate_json(config_json)

            if not self.config.appkey:
                await self.send_asr_error(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message="appkey is required",
                    )
                )

            if not self.config.akid:
                await self.send_asr_error(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message="akid is required",
                    )
                )

            if not self.config.aksecret:
                await self.send_asr_error(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message="aksecret is required",
                    )
                )

        except Exception as e:
            self.ten_env.log_error(f"Error parsing config: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                )
            )

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)
        await self.start_connection()

    def _on_start(self, message, *_):
        self.ten_env.log_info(f"aliyun_asr event callback on_start: {message}")
        self.connected = True

    def _on_close(self, *args, **kwargs):
        self.ten_env.log_info(
            f"aliyun_asr event callback on_close: {args}, {kwargs}"
        )
        self.connected = False
        if not self.stopped:
            self.ten_env.log_warn(
                "aliyun_asr connection closed unexpectedly. Reconnecting..."
            )
            asyncio.create_task(self._handle_reconnect())

    def _on_message(self, result, *_):
        if not result:
            self.ten_env.log_warn("Received empty result.")
            return

        try:
            # Parse the JSON string once
            result_data = json.loads(result)  # Assuming result is a JSON string

            if "payload" not in result_data:
                self.ten_env.log_warn("Received malformed result.")
                return

            sentence = result_data.get("payload", {}).get("result", "")

            if len(sentence) == 0:
                return

            is_final = (
                result_data.get("header", {}).get("name") == "SentenceEnd"
            )
            self.ten_env.log_info(
                f"aliyun_asr got sentence: [{sentence}], is_final: {is_final}"
            )

            asr_result = ASRResult(
                text=sentence,
                final=is_final,
                start_ms=-1,
                duration_ms=-1,
                language="zh-CN",
                words=[],
            )

            assert self.loop is not None
            self.loop.create_task(self.send_asr_result(asr_result))
        except Exception as e:
            self.ten_env.log_error(f"Error processing message: {e}")

    def _on_error(self, message, *_):
        self.ten_env.log_error(f"aliyun_asr event callback on_error: {message}")

        asyncio.create_task(
            self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=message,
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code="1",
                    message=message,
                ),
            )
        )

    async def start_connection(self) -> None:
        self.ten_env.log_info("start and listen aliyun")
        assert self.config is not None

        try:
            await self.stop_connection()

            token = nls.token.getToken(self.config.akid, self.config.aksecret)
            self.client = nls.NlsSpeechTranscriber(
                url=self.config.api_url,
                appkey=self.config.appkey,
                token=token,
                on_start=self._on_start,
                on_sentence_begin=self._on_message,
                on_sentence_end=self._on_message,
                on_result_changed=self._on_message,
                on_completed=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close,
                callback_args=[],
            )

            # connect to websocket
            self.client.start(
                aformat="pcm",
                enable_intermediate_result=True,
                enable_punctuation_prediction=True,
                enable_inverse_text_normalization=True,
            )

            self.ten_env.log_info("successfully connected to aliyun asr")
        except Exception as e:
            self.ten_env.log_error(f"Error starting aliyun connection: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                )
            )
            await self._handle_reconnect()

    async def stop_connection(self) -> None:
        try:
            if self.client:
                self.client.stop()
                self.client = None
                self.connected = False
                self.ten_env.log_info("aliyun connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping aliyun connection: {e}")

    def is_connected(self) -> bool:
        return self.connected

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id

        if self.client is None:
            return False

        self.client.send_audio(frame.get_buf())
        return True

    async def finalize(self, session_id: str | None) -> None:
        pass

    def input_audio_sample_rate(self) -> int:
        return 16000  # Default sample rate for Aliyun ASR
