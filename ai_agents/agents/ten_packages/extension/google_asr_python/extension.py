#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#


import json
from ten_runtime import (
    AudioFrame,
    AsyncTenEnv,
)

from typing_extensions import override
from ten_ai_base.asr import (
    AsyncASRBaseExtension,
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
)
from ten_ai_base.message import ModuleError, ModuleType

from .config import GoogleASRConfig
from .google_asr_client import GoogleASRClient


class GoogleASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.client: GoogleASRClient | None = None
        self.config: GoogleASRConfig | None = None
        self.session_id: str | None = None

    @override
    def vendor(self) -> str:
        """Returns the name of the ASR service provider."""
        return "google"

    async def _on_asr_result(self, result: ASRResult):
        """Callback for handling ASR results from the client."""
        await self.send_asr_result(self.session_id, result)

    async def _on_asr_error(self, code: int, message: str):
        """Callback for handling errors from the client."""
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=code,
                message=message,
            )
        )

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Loads the configuration from property.json."""
        await super().on_init(ten_env)
        try:
            config_json, _ = await ten_env.get_property_to_json()
            config_dict = json.loads(config_json)
            self.config = GoogleASRConfig(**config_dict.get("params", {}))

            # Debug: Print ADC authentication information
            if self.config.project_id:
                ten_env.log_info(f"Google ASR Project ID: {self.config.project_id}")
            else:
                ten_env.log_info("Google ASR Project ID not set, will use ADC default project")

            if self.config.adc_credentials_path:
                ten_env.log_info(f"Google ASR ADC credentials path: {self.config.adc_credentials_path}")
            else:
                ten_env.log_info("Google ASR ADC credentials path not set, will use environment variable or default ADC")

            is_valid, error_msg = self.config.validate_config()
            if not is_valid:
                raise Exception(
                    f"Google ASR config validation failed: {error_msg}"
                )
            ten_env.log_info("Google ASR configuration loaded and validated.")
        except Exception as e:
            ten_env.log_error(f"Error during Google ASR initialization: {e}")
            raise Exception(f"Initialization failed: {e}") from e

    @override
    async def start_connection(self) -> None:
        """Starts the connection to Google ASR."""
        if self.connected:
            self.ten_env.log_warn("Connection already started.")
            return

        if not self.config or not self.ten_env:
            raise Exception(
                "Extension not initialized properly. Config or ten_env is missing."
            )

        self.ten_env.log_info("Starting Google ASR connection...")
        try:
            self.client = GoogleASRClient(
                config=self.config,
                ten_env=self.ten_env,
                on_result_callback=self._on_asr_result,
                on_error_callback=self._on_asr_error,
            )
            await self.client.start()
            self.connected = True
            self.ten_env.log_info("Google ASR connection started successfully.")
        except Exception as e:
            self.ten_env.log_error(
                f"Failed to start Google ASR connection: {e}"
            )
            self.connected = False
            await self._on_asr_error(500, f"Failed to start connection: {e}")

    @override
    async def stop_connection(self) -> None:
        """Stops the connection to Google ASR."""
        if not self.connected:
            self.ten_env.log_warn("Connection already stopped.")
            return

        self.ten_env.log_info("Stopping Google ASR connection...")
        if self.client:
            await self.client.stop()
        self.client = None
        self.connected = False
        self.ten_env.log_info("Google ASR connection stopped.")

    @override
    def is_connected(self) -> bool:
        """Checks the connection status."""
        return self.connected and self.client is not None

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """Sends an audio frame for recognition."""
        if not self.is_connected() or not self.client:
            self.ten_env.log_warn("Cannot send audio, client not connected.")
            return False

        if self.session_id != session_id:
            self.session_id = session_id

        buf = frame.lock_buf()
        try:
            await self.client.send_audio(bytes(buf))
        finally:
            frame.unlock_buf(buf)

        return True

    @override
    async def finalize(self, session_id: str | None) -> None:
        """Finalizes the recognition for the current utterance."""
        if not self.is_connected() or not self.client:
            self.ten_env.log_warn("Cannot finalize, client not connected.")
            return

        self.ten_env.log_info(f"Finalizing ASR for session: {session_id}")
        await self.client.finalize()
        await self.send_asr_finalize_end()

    @override
    def input_audio_sample_rate(self) -> int:
        """Returns the expected audio sample rate."""
        if not self.config:
            return 16000  # Default value
        return self.config.sample_rate

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        """Defines the audio buffer strategy."""
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)
