from datetime import datetime
import os
from typing import Optional

from typing_extensions import override
from .const import DUMP_FILE_NAME
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
    ModuleType,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)

from ten_ai_base.dumper import Dumper
from .reconnect_manager import ReconnectManager
from .config import SpeechmaticsASRConfig
from .asr_client import SpeechmaticsASRClient
from .language_utils import normalized_language


class SpeechmaticsASRExtension(AsyncASRBaseExtension):
    """Speechmatics ASR Extension"""

    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.audio_dumper: Optional[Dumper] = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0
        self.is_finalize_disconnect: bool = False

        self.client: SpeechmaticsASRClient | None = None
        self.config: SpeechmaticsASRConfig | None = None

        # Reconnection manager
        self.reconnect_manager: Optional[ReconnectManager] = None

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        """Deinitialize extension"""
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    def vendor(self) -> str:
        """Get ASR vendor name"""
        return "speechmatics"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Initialize extension"""
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = SpeechmaticsASRConfig.model_validate_json(config_json)
            ten_env.log_info(f"Speechmatics ASR config: {self.config}")
            self.config.update(self.config.params)
            ten_env.log_info(
                f"Speechmatics ASR config: {self.config.to_json(sensitive_handling=True)}"
            )
            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)

        except Exception as e:
            ten_env.log_error(f"Invalid Speechmatics ASR config: {e}")
            self.config = SpeechmaticsASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        """Start ASR connection"""
        assert self.config is not None
        self.ten_env.log_info("Starting Speechmatics ASR connection")

        try:
            # Check required credentials
            if not self.config.key or self.config.key.strip() == "":
                error_msg = "Speechmatics API key is required but not provided or is empty"
                self.ten_env.log_error(error_msg)
                await self.send_asr_error(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=error_msg,
                    ),
                )
                return

            # Stop existing connection
            await self.stop_connection()
            # Start audio dumper
            if self.audio_dumper:
                await self.audio_dumper.start()

            self.client = SpeechmaticsASRClient(
                self.config,
                self.ten_env,
                self.audio_timeline,
            )
            self.client.on_asr_open = self.on_asr_open
            self.client.on_asr_close = self.on_asr_close
            self.client.on_asr_result = self.on_asr_result
            self.client.on_asr_error = self.on_asr_error
            return await self.client.start()

        except Exception as e:
            self.ten_env.log_error(
                f"Failed to start Speechmatics ASR connection: {e}"
            )
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def on_asr_open(self) -> None:
        """Handle callback when connection is established"""
        self.ten_env.log_info("Speechmatics ASR connection opened")
        self.connected = True

        # Notify reconnect manager of successful connection
        if self.reconnect_manager and self.connected:
            self.reconnect_manager.mark_connection_successful()

        # Reset timeline and audio duration
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()

    async def on_asr_result(self, message_data: ASRResult) -> None:
        """Handle recognition result callback"""
        self.ten_env.log_info(f"Speechmatics ASR result: {message_data}")

        await self._handle_asr_result(
            text=message_data.text,
            final=message_data.final,
            start_ms=message_data.start_ms,
            duration_ms=message_data.duration_ms,
            language=normalized_language(message_data.language),
        )

    async def on_asr_error(
        self, error_msg: str, error_code: Optional[int] = None
    ) -> None:
        """Handle error callback"""
        self.ten_env.log_error(
            f"Speechmatics ASR error: {error_msg} code: {error_code}"
        )
        await self._handle_reconnect()

        # Send error information
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=error_msg,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(error_code) if error_code else "unknown",
                message=error_msg,
            ),
        )

    async def on_asr_close(self) -> None:
        """Handle callback when connection is closed"""
        self.ten_env.log_debug("Speechmatics ASR connection closed")
        self.connected = False

        if self.is_finalize_disconnect:
            self.ten_env.log_warn(
                "Speechmatics ASR connection closed unexpectedly. Reconnecting..."
            )
            await self._handle_reconnect()

    @override
    async def finalize(self, _session_id: Optional[str]) -> None:
        """Finalize recognition"""
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_debug(
            f"Speechmatics ASR finalize start at {self.last_finalize_timestamp}"
        )

        if self.config.drain_mode == "mute_pkg":
            return await self._handle_finalize_mute_pkg()
        return await self._handle_finalize_disconnect()

    async def _handle_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int = 0,
        duration_ms: int = 0,
        language: str = "",
    ):
        """Process ASR recognition result"""
        assert self.config is not None

        asr_result = ASRResult(
            text=text,
            final=final,
            start_ms=start_ms,
            duration_ms=duration_ms,
            language=language,
            words=[],
        )

        if final:
            await self._finalize_end()

        await self.send_asr_result(asr_result)

    async def _handle_finalize_disconnect(self):
        """Handle disconnect mode finalization"""
        if self.client:
            self.is_finalize_disconnect = True
            await self.client.internal_drain_disconnect()
            self.ten_env.log_debug(
                "Speechmatics ASR finalize disconnect completed"
            )

    async def _handle_finalize_mute_pkg(self):
        """Handle mute package mode finalization"""
        if self.client:
            self.is_finalize_disconnect = True
            await self.client.internal_drain_mute_pkg()
            self.ten_env.log_debug(
                "Speechmatics ASR finalize mute pkg completed"
            )

    async def _handle_reconnect(self):
        """Handle reconnection"""
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        # Check if retry is still possible
        if not self.reconnect_manager.can_retry():
            self.ten_env.log_warn("No more reconnection attempts allowed")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message="No more reconnection attempts allowed",
                )
            )
            return

        # Attempt reconnection
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
                f"Speechmatics ASR finalize end at {timestamp}, latency: {latency}ms"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def stop_connection(self) -> None:
        """Stop ASR connection"""
        try:
            if self.client:
                await self.client.stop()
                self.client = None

            self.connected = False
            self.ten_env.log_info("Speechmatics ASR connection stopped")

        except Exception as e:
            self.ten_env.log_error(
                f"Error stopping Speechmatics ASR connection: {e}"
            )

    @override
    def is_connected(self) -> bool:
        """Check connection status"""
        is_connected: bool = (
            self.connected
            and self.client is not None
            and self.client.is_connected()
            and not self.is_finalize_disconnect
        )
        return is_connected

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        """Buffer strategy configuration"""
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        """Input audio sample rate"""
        assert self.config is not None
        return self.config.sample_rate

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: Optional[str]
    ) -> bool:
        """Send audio data"""
        assert self.config is not None

        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)

            # Dump audio data
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)

            # Update timeline
            self.audio_timeline.add_user_audio(
                int(len(audio_data) / (self.config.sample_rate / 1000 * 2))
            )

            if self.client:
                await self.client.recv_audio_frame(frame, _session_id)

            frame.unlock_buf(buf)
            return True

        except Exception as e:
            self.ten_env.log_error(
                f"Error sending audio to Speechmatics ASR: {e}"
            )
            frame.unlock_buf(buf)
            return False
