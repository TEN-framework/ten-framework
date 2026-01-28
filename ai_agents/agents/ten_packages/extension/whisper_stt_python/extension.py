#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#

import os
from datetime import datetime
from typing import Optional
from typing_extensions import override

from ten_runtime import AsyncTenEnv, AudioFrame
from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)
from ten_ai_base.const import (
    LOG_CATEGORY_VENDOR,
    LOG_CATEGORY_KEY_POINT,
)
from ten_ai_base.dumper import Dumper

from .config import WhisperSTTConfig
from .const import MODULE_NAME_ASR, DUMP_FILE_NAME
from .reconnect_manager import ReconnectManager
from .whisper_client import WhisperClient


class WhisperSTTExtension(AsyncASRBaseExtension):
    """Whisper STT Extension using faster-whisper"""

    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[WhisperSTTConfig] = None
        self.client: Optional[WhisperClient] = None
        self.audio_dumper: Optional[Dumper] = None
        self.reconnect_manager: Optional[ReconnectManager] = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0

    @override
    def vendor(self) -> str:
        """Get ASR vendor name"""
        return "whisper"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = WhisperSTTConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            # Initialize audio dumper if enabled
            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()

        except Exception as e:
            ten_env.log_error(f"Invalid Whisper config: {e}")
            self.config = WhisperSTTConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    async def start_connection(self) -> None:
        """Start ASR connection"""
        assert self.config is not None
        self.ten_env.log_info("Starting Whisper connection")

        try:
            # Stop existing connection
            if self.is_connected():
                await self.stop_connection()

            # Get configuration parameters
            model_size = self.config.params.get("model", "base")
            device = self.config.params.get("device", "cpu")
            compute_type = self.config.params.get("compute_type", "int8")
            language = self.config.params.get("language")
            task = self.config.params.get("task", "transcribe")
            sample_rate = self.config.params.get("sample_rate", 16000)

            # Create client
            self.client = WhisperClient(
                model_size=model_size,
                device=device,
                compute_type=compute_type,
                language=language,
                task=task,
                sample_rate=sample_rate,
                on_result_callback=self._on_result,
                on_error_callback=self._on_error,
                logger=self.ten_env,
            )

            await self.client.connect()

            # Mark connection successful
            if self.reconnect_manager:
                self.reconnect_manager.mark_connection_successful()

            # Reset timeline
            self.sent_user_audio_duration_ms_before_last_reset += (
                self.audio_timeline.get_total_user_audio_duration()
            )
            self.audio_timeline.reset()

            self.ten_env.log_info(
                "Whisper connection established",
                category=LOG_CATEGORY_VENDOR,
            )

        except Exception as e:
            self.ten_env.log_error(f"Failed to start Whisper connection: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def stop_connection(self) -> None:
        """Stop ASR connection"""
        self.ten_env.log_info("Stopping Whisper connection")
        try:
            if self.client:
                await self.client.disconnect()
                self.client = None
            self.ten_env.log_info("Whisper connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping Whisper connection: {e}")

    @override
    def is_connected(self) -> bool:
        """Check connection status"""
        return self.client is not None and self.client.is_connected()

    @override
    def input_audio_sample_rate(self) -> int:
        """Input audio sample rate"""
        assert self.config is not None
        return self.config.params.get("sample_rate", 16000)

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        """Buffer strategy configuration"""
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        """Send audio data"""
        if not self.is_connected() or not self.client:
            return False

        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)

            # Dump audio data
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)

            # Send to client
            await self.client.send_audio(audio_data)

            frame.unlock_buf(buf)
            return True

        except Exception as e:
            self.ten_env.log_error(f"Error sending audio to Whisper: {e}")
            frame.unlock_buf(buf)
            return False

    @override
    async def finalize(self, _session_id: str | None) -> None:
        """Finalize recognition"""
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_debug(
            f"Whisper finalize start at {self.last_finalize_timestamp}"
        )

        finalize_mode = self.config.finalize_mode
        if finalize_mode == "disconnect":
            await self._handle_finalize_disconnect()
        elif finalize_mode == "silence":
            await self._handle_finalize_silence()
        else:
            raise ValueError(f"invalid finalize mode: {finalize_mode}")

    async def _handle_finalize_disconnect(self) -> None:
        """Handle disconnect mode finalization"""
        if self.client:
            await self.client.finalize()
            self.ten_env.log_debug(
                "Whisper finalize completed (disconnect mode)"
            )

    async def _handle_finalize_silence(self) -> None:
        """Handle silence mode finalization"""
        if self.client and self.config:
            # Process any remaining audio
            await self.client.finalize()
            self.ten_env.log_debug("Whisper finalize completed (silence mode)")

    async def _handle_reconnect(self) -> None:
        """Handle reconnection"""
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        success = await self.reconnect_manager.handle_reconnect(
            connection_func=self.start_connection,
            error_handler=self.send_asr_error,
        )

        if success:
            self.ten_env.log_debug(
                "Reconnection attempt initiated successfully"
            )
        else:
            info = self.reconnect_manager.get_attempts_info()
            self.ten_env.log_debug(
                f"Reconnection attempt failed. Status: {info}"
            )

    async def _finalize_end(self) -> None:
        """Handle finalization end logic"""
        if self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"Whisper finalize end at {timestamp}, latency: {latency}ms"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def _on_result(
        self,
        text: str,
        start_ms: int,
        duration_ms: int,
        language: str,
        final: bool,
    ) -> None:
        """Handle recognition result callback"""
        try:
            if not text:
                return

            # Calculate actual start time using audio timeline
            actual_start_ms = int(
                self.audio_timeline.get_audio_duration_before_time(start_ms)
                + self.sent_user_audio_duration_ms_before_last_reset
            )

            # Create ASR result
            asr_result = ASRResult(
                text=text,
                final=final,
                start_ms=actual_start_ms,
                duration_ms=duration_ms,
                language=(
                    self.config.normalized_language if self.config else language
                ),
                words=[],
            )

            await self.send_asr_result(asr_result)

            # Handle finalize end
            if final:
                await self._finalize_end()

        except Exception as e:
            self.ten_env.log_error(f"Error processing Whisper result: {e}")

    async def _on_error(self, error_msg: str) -> None:
        """Handle error callback"""
        self.ten_env.log_error(
            f"vendor_error: {error_msg}",
            category=LOG_CATEGORY_VENDOR,
        )

        # Send error information
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=error_msg,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code="unknown",
                message=error_msg,
            ),
        )

        # Attempt reconnection
        await self._handle_reconnect()
