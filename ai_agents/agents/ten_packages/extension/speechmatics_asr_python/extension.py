#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from ten_ai_base.asr import AsyncASRBaseExtension
from ten_ai_base.transcription import UserTranscription
from ten_runtime import (
    AsyncTenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
)
from .asr_client import SpeechmaticsASRClient, SpeechmaticsASRConfig


class SpeechmaticsASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.client: SpeechmaticsASRClient = None
        self.config: SpeechmaticsASRConfig = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd: {cmd_name}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def start_connection(self) -> None:
        if self.client is None:

            if self.config is None:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = SpeechmaticsASRConfig.model_validate_json(
                    config_json
                )
                self.ten_env.log_info(f"config: {self.config}")

                if not self.config.key:
                    self.ten_env.log_error("get property key failed")
                    return

            self.client = SpeechmaticsASRClient(self.config, self.ten_env)
            self.client.on_transcription = self._on_transcription
            return await self.client.start()

    async def stop_connection(self) -> None:
        return await self.client.stop()

    async def finalize(self, session_id: str | None) -> None:
        if self.config.drain_mode == "mute_pkg":
            return await self.client.internal_drain_mute_pkg()
        return await self.client.internal_drain_disconnect()

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        await self.client.recv_audio_frame(frame, session_id)
        return True

    def is_connected(self) -> bool:
        return bool(
            self.client
            and getattr(self.client.client, "session_running", False)
        )

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def _on_transcription(
        self,
        user_transcription: UserTranscription,
    ) -> None:
        # Convert the transcription to UserTranscription and send
        self.ten_env.log_info(
            f"Transcription received: {user_transcription.text}"
        )
        await self.send_asr_transcription(user_transcription)

    async def _on_error(
        self,
        error: Exception,
        vendor_info: dict | None = None,
    ) -> None:
        # Handle errors from the ASR client
        self.ten_env.log_error(f"ASR error: {error}")
        await self.send_asr_error(error, vendor_info)
