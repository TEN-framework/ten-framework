#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from typing_extensions import override
from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorVendorInfo,
)
from ten_ai_base.struct import ASRResult
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)
from .asr_client import SpeechmaticsASRClient, SpeechmaticsASRConfig


class SpeechmaticsASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.client: SpeechmaticsASRClient | None = None
        self.config: SpeechmaticsASRConfig | None = None

    def vendor(self) -> str:
        return "speechmatics"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        config_json, _ = await self.ten_env.get_property_to_json("")
        try:
            self.config = SpeechmaticsASRConfig.model_validate_json(config_json)
        except Exception as e:
            self.ten_env.log_error(f"get property failed: {e}")
            return

        if not self.config.key:
            self.ten_env.log_error("get property key failed")
            return

    async def start_connection(self) -> None:
        if self.client is None:
            assert self.config is not None

            self.client = SpeechmaticsASRClient(self.config, self.ten_env)
            self.client.on_asr_result = self._on_asr_result
            self.client.on_error = self._on_error
            return await self.client.start()

    async def stop_connection(self) -> None:
        if self.client is None:
            return

        return await self.client.stop()

    async def finalize(self, session_id: str | None) -> None:
        if self.client is None:
            return

        if self.config is None:
            return

        if self.config.drain_mode == "mute_pkg":
            return await self.client.internal_drain_mute_pkg()
        return await self.client.internal_drain_disconnect()

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        if self.client is None:
            return False

        await self.client.recv_audio_frame(frame, session_id)
        return True

    def is_connected(self) -> bool:
        return bool(
            self.client
            and getattr(self.client.client, "session_running", False)
        )

    def input_audio_sample_rate(self) -> int:
        if self.config is None:
            return 16000

        return self.config.sample_rate

    async def _on_asr_result(
        self,
        asr_result: ASRResult,
    ) -> None:
        self.ten_env.log_info(f"ASR result received: {asr_result.text}")
        await self.send_asr_result(asr_result)

    async def _on_error(
        self,
        error: ModuleError,
        vendor_info: ModuleErrorVendorInfo | None = None,
    ) -> None:
        # Handle errors from the ASR client
        self.ten_env.log_error(f"ASR error: {error.message}")

        await self.send_asr_error(error, vendor_info)
