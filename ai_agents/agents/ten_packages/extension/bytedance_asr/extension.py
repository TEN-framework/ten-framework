import asyncio
import os
from datetime import datetime
from typing import Any
from typing_extensions import override
from pydantic import BaseModel, Field
from ten_ai_base.asr import (
    AsyncASRBaseExtension,
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ErrorMessage,
    ModuleType,
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)
from ten_ai_base.transcription import UserTranscription

# from ten_ai_base.struct import ASRResult, ASRWord
from ten_ai_base.utils import encrypt
from ten_ai_base.timeline import AudioTimeline
from ten_runtime import (
    AsyncTenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
    ten_env,
)
from .bytedance_asr import AsrWsClient
from .config import BytedanceASRConfig
from .const import (
    FINALIZE_MODE_DISCONNECT,
    FINALIZE_MODE_MUTE_PKG,
    DUMP_FILE_NAME,
    MODULE_NAME_ASR,
    BYTEDANCE_ERROR_CODES,
    RECONNECTABLE_ERROR_CODES,
    FATAL_ERROR_CODES,
    DEFAULT_WORKFLOW,
)


class BytedanceASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        # Connection state
        self.connected: bool = False
        self.client: AsrWsClient | None = None
        self.config: BytedanceASRConfig | None = None
        self.timeline: AudioTimeline = AudioTimeline()
        self.last_finalize_timestamp: int = 0
        self.audio_dumper: Dumper | None = None
        self.ten_env: Any | None = None

        # Reconnection parameters (will be set from config)
        self.max_retries: int = 5
        self.base_delay: float = 0.3
        self.attempts: int = 0
        self.stopped: bool = False  # Whether extension is stopped
        self.last_fatal_error: int | None = (
            None  # Track fatal errors to prevent unnecessary reconnection
        )

    @override
    def vendor(self) -> str:
        """Get the name of the ASR vendor."""
        return "bytedance"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env  # Store ten_env reference for reconnection

        config_json, _ = await ten_env.get_property_to_json()

        try:
            self.config = BytedanceASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}"
            )

            # Set reconnection parameters from config
            self.max_retries = self.config.max_retries
            self.base_delay = self.config.base_delay

            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()

            self.timeline.reset()
            self.last_finalize_timestamp = 0
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = BytedanceASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor="bytedance",
                    code="CONFIG_ERROR",
                    message=f"Configuration validation failed: {str(e)}",
                ),
            )

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_json = cmd.to_json()
        ten_env.log_info(f"on_cmd json: {cmd_json}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        await ten_env.return_result(cmd_result)

    async def _handle_reconnect(self, ten_env: Any | None = None) -> None:
        """Handle reconnection logic with exponential backoff strategy."""
        # Use provided ten_env or stored ten_env
        env = ten_env or getattr(self, "ten_env", None)
        if not env:
            # Unable to log since no ten_env is available
            return

        if self.attempts >= self.max_retries:
            env.log_error(
                f"Max retries ({self.max_retries}) reached, stopping reconnection attempts"
            )
            return

        # Increment retry count and calculate exponential backoff delay
        self.attempts += 1
        delay = self.base_delay * (2 ** (self.attempts - 1))

        env.log_info(
            f"=== Starting reconnection === Attempt {self.attempts}, waiting {delay:.3f}s"
        )
        await asyncio.sleep(delay)
        env.log_info("=== Delay completed, starting reconnection ===")

        try:
            await self.stop_connection()
            await self.start_connection()
            # Don't reset retry count here, wait for non-1001 error codes or normal messages
            env.log_info(
                "=== Reconnection established, waiting for validation ==="
            )
        except Exception as e:
            env.log_error(
                f"=== Reconnection failed === Attempt {self.attempts} failed: {e}"
            )
            if self.attempts < self.max_retries and not self.stopped:
                # Continue retrying if we have attempts left and extension isn't stopped
                env.log_info(
                    f"=== Preparing next retry === Progress: {self.attempts}/{self.max_retries}"
                )
                asyncio.create_task(self._handle_reconnect(env))
            else:
                env.log_error("=== All retry attempts failed ===")

    async def on_finalize_complete_callback(self) -> None:
        """Callback function when ASR finalize is completed."""
        self.ten_env.log_info(
            "ASR finalize completed, sending finalize end signal"
        )
        try:
            await self.send_asr_finalize_end()
            self.ten_env.log_info("Sent asr_finalize_end signal from callback")
        except Exception as e:
            self.ten_env.log_error(
                f"Error sending asr_finalize_end signal from callback: {e}"
            )

    @override
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

        # if self.audio_dumper:
        #     await self.audio_dumper.start()

        async def on_message(result):
            # self.ten_env.log_info(f"on_message result: {result}")
            if (
                not result
                or "text" not in result[0]
                or "utterances" not in result[0]
            ):
                return

            sentence = result[0]["text"]
            start_ms = result[0]["utterances"][0].get("start_time", 0)
            end_ms = result[0]["utterances"][0].get("end_time", 0)

            if len(sentence) == 0:
                return

            is_final = result[0]["utterances"][0].get(
                "definite", False
            )  # Use get to avoid KeyError
            self.ten_env.log_info(
                f"bytedance_asr got sentence: [{sentence}], is_final: {is_final}"
            )

            # Received normal message, consider connection successful, reset retry count
            if self.attempts > 0:
                self.ten_env.log_info(
                    "=== Received normal message, connection successful, resetting retry count ==="
                )
                self.attempts = 0

            # Convert to ASRResult
            asr_result = ASRResult(
                text=sentence,
                final=is_final,
                start_ms=start_ms,
                duration_ms=end_ms - start_ms,
                language=self.config.language if self.config else "zh-CN",
                metadata={
                    "session_id": self.session_id,
                },
                words=[],
            )

            await self.send_asr_result(asr_result)

        async def on_error(error_code: int, error_msg: str):
            """Callback function to handle ASR error codes"""
            self.ten_env.log_error(f"ASR error: {error_code} - {error_msg}")
            error_message = ModuleError(
                module=ModuleType.ASR,
                code=int(ModuleErrorCode.NON_FATAL_ERROR),
                message=error_msg,
            )

            # Map error code to descriptive name using constants
            error_code_name = BYTEDANCE_ERROR_CODES.get(
                error_code, f"UNKNOWN_ERROR_{error_code}"
            )

            # Create vendor_info with Bytedance-specific error information
            vendor_info = ModuleErrorVendorInfo(
                vendor="bytedance",
                code=error_code_name,
                message=f"Bytedance ASR error {error_code}: {error_msg}",
            )

            await self.send_asr_error(error_message, vendor_info)

            # Check if this is a fatal error that shouldn't trigger reconnection
            if error_code in FATAL_ERROR_CODES:
                self.last_fatal_error = error_code
                self.ten_env.log_info(
                    f"=== Received fatal error code {error_code}, closing connection to prevent further errors ==="
                )
                # Close connection immediately for fatal errors to prevent continuous error logs
                await self.stop_connection()
                return

            # Special handling for connection errors (2001) - check if due to previous fatal error
            if error_code == 2001 and self.last_fatal_error:
                self.ten_env.log_info(
                    f"=== Connection closed due to previous fatal error {self.last_fatal_error}, skipping reconnection ==="
                )
                self.last_fatal_error = None  # Clear the flag
                return  # Don't proceed with reconnection logic

            # Trigger reconnection mechanism for reconnectable error codes
            if error_code in RECONNECTABLE_ERROR_CODES and not self.stopped:
                self.ten_env.log_info(
                    f"=== Received reconnectable error code {error_code}, triggering reconnection === Current retry count: {self.attempts}"
                )
                asyncio.create_task(self._handle_reconnect(self.ten_env))
            # Reset retry count for success codes and non-fatal errors
            elif error_code in [1000, 0] or (
                error_code not in RECONNECTABLE_ERROR_CODES
                and error_code < 2000
            ):
                if self.attempts > 0:
                    self.ten_env.log_info(
                        f"=== Received success/non-fatal error code ({error_code}), resetting retry count ==="
                    )
                    self.attempts = 0
                # Clear fatal error flag on success
                self.last_fatal_error = None
            else:
                # Other unknown error codes, log but don't handle
                self.ten_env.log_info(
                    f"=== Received unknown error code ({error_code}), no action taken ==="
                )

        try:
            self.client = AsrWsClient(
                ten_env=self.ten_env,
                cluster=self.config.cluster,
                appid=self.config.appid,
                token=self.config.token,
                api_url=self.config.api_url,
                workflow=self.config.workflow,
                handle_received_message=on_message,
                on_finalize_complete=self.on_finalize_complete_callback,
                on_error=on_error,
            )

            # connect to websocket
            await self.client.start()
            self.connected = True
            # Clear fatal error flag on successful connection
            self.last_fatal_error = None
        except Exception as e:
            self.ten_env.log_error(f"Failed to start Bytedance ASR client: {e}")
            error_message = ModuleError(
                module=ModuleType.ASR,
                code=int(ModuleErrorCode.FATAL_ERROR),
                message=str(e),
            )

            vendor_info = ModuleErrorVendorInfo(
                vendor="bytedance",
                code="CONNECTION_ERROR",
                message=f"Failed to establish WebSocket connection: {str(e)}",
            )

            await self.send_asr_error(error_message, vendor_info)
            # Trigger reconnection on connection failure using error code 1007 (engine internal error)
            if not self.stopped:
                self.ten_env.log_info(
                    "=== Initial connection failed, triggering reconnection ==="
                )
                asyncio.create_task(self._handle_reconnect(self.ten_env))

    @override
    async def stop_connection(self) -> None:
        self.stopped = True  # Mark extension as stopped
        if self.client:
            await self.client.finish()
            self.client = None
            self.connected = False

        # Stop audio dumper when connection stops
        if self.audio_dumper:
            try:
                await self.audio_dumper.stop()
                self.ten_env.log_info("Audio dumper stopped successfully")
            except Exception as e:
                self.ten_env.log_error(f"Error stopping audio dumper: {e}")
            finally:
                self.audio_dumper = None

        # Reset reconnection state when stopping
        self.attempts = 0
        self.last_fatal_error = None

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        # Check if connection is closed due to fatal error
        if self.last_fatal_error:
            self.ten_env.log_info(
                f"Skipping audio send due to previous fatal error {self.last_fatal_error}"
            )
            return False

        self.session_id = session_id
        buf = frame.lock_buf()
        try:
            if self.audio_dumper:
                audio_data = bytes(buf)
                await self.audio_dumper.push_bytes(audio_data)

            self.timeline.add_user_audio(
                int(len(buf) / (self.input_audio_sample_rate() / 1000 * 2))
            )
            if self.client:
                await self.client.send(bytes(buf))
                return True
            return False
        finally:
            frame.unlock_buf(buf)

    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_debug(
            f"KEYPOINT finalize start at {self.last_finalize_timestamp}"
        )

        if not self.client or not self.connected:
            self.ten_env.log_warn("ASR client not connected, skipping finalize")
            return

        try:
            # Send finalize signal to ASR client
            await self.client.finalize()

            # Use timeout from configuration
            finalize_timeout = self.config.finalize_timeout

            # Wait for final result or timeout
            finalize_success = await self.client.wait_for_finalize(
                finalize_timeout
            )

            if finalize_success:
                self.ten_env.log_info("ASR finalize completed successfully")
            else:
                self.ten_env.log_warn(
                    "ASR finalize timeout, proceeding with cleanup"
                )

            # Send finalize_end signal regardless of timeout
            try:
                await self.send_asr_finalize_end()
                self.ten_env.log_info("Sent asr_finalize_end signal")
            except Exception as e:
                self.ten_env.log_error(
                    f"Error sending asr_finalize_end signal: {e}"
                )

            # Handle connection based on finalize_mode
            if self.config.finalize_mode == FINALIZE_MODE_DISCONNECT:
                self.ten_env.log_info(
                    "Disconnecting ASR client due to finalize mode"
                )
                await self.stop_connection()
            elif self.config.finalize_mode == FINALIZE_MODE_MUTE_PKG:
                self.ten_env.log_info(
                    "Keeping ASR connection but muting audio packages"
                )
                # In mute_pkg mode, keep connection but stop sending audio
                # Client will continue processing received audio until final result
            else:
                self.ten_env.log_warn(
                    f"Unknown finalize mode: {self.config.finalize_mode}"
                )

        except Exception as e:
            self.ten_env.log_error(f"Error during ASR finalize: {e}")
            # Ensure connection is properly cleaned up even if finalize fails
            if self.config.finalize_mode == FINALIZE_MODE_DISCONNECT:
                await self.stop_connection()

    @override
    def is_connected(self) -> bool:
        return self.connected

    @override
    def input_audio_sample_rate(self) -> int:
        return 16000

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)
