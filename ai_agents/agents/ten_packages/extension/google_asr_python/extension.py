#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from datetime import datetime
import asyncio
import os
from ten_runtime import (
    AudioFrame,
    AsyncTenEnv,
)

from ten_ai_base.const import (
    LOG_CATEGORY_VENDOR,
    LOG_CATEGORY_KEY_POINT,
)

from typing_extensions import override
from ten_ai_base.asr import (
    AsyncASRBaseExtension,
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
)

from ten_ai_base.message import (
    ModuleError,
    ModuleType,
    ModuleErrorVendorInfo,
    ModuleErrorCode,
)
from ten_ai_base.dumper import Dumper

from .config import GoogleASRConfig
from .google_asr_client import GoogleASRClient
from .reconnect_manager import ReconnectManager


class GoogleASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.client: GoogleASRClient | None = None
        self.config: GoogleASRConfig | None = None
        self.session_id: str | None = None
        self.audio_dumper: Dumper | None = None
        self.reconnect_manager: ReconnectManager | None = None
        self.stopped: bool = False
        self._reconnect_task: asyncio.Task | None = None
        self.last_finalize_timestamp: int = 0

    @override
    def vendor(self) -> str:
        """Returns the name of the ASR service provider."""
        return "google"

    async def _on_asr_result(self, result: ASRResult):
        """Callback for handling ASR results from the client."""
        await self.send_asr_result(result)

    async def _on_asr_error(self, code: int, message: str):
        """Callback for handling errors from the client."""
        self.ten_env.log_error(
            f"vendor_error: code: {code}, reason: {message}",
            category=LOG_CATEGORY_VENDOR,
        )
        severity = self._map_to_module_error_code(code, message)
        await self.send_asr_error(
            ModuleError(
                module=ModuleType.ASR,
                code=severity,
                message=message,
            ),
            ModuleErrorVendorInfo(
                vendor="google",
                code=str(code),
                message=message,
            ),
        )
        # For non-fatal errors, attempt reconnect in background
        if (
            severity == ModuleErrorCode.NON_FATAL_ERROR.value
            and not self.stopped
        ):
            try:
                # Skip reconnect for client-side normal shutdowns
                if (code == 1) or ("CANCELLED" in (message or "").upper()):
                    self.ten_env.log_debug(
                        "Non-fatal CANCELLED received; skip reconnect."
                    )
                    return

                # Mark as disconnected and stop client before reconnecting
                self.connected = False
                if self.client:
                    try:
                        await self.client.stop(self.session_id)
                    except Exception:
                        ...
                    self.client = None
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"Scheduling reconnect due to non-fatal error ({code})"
                    )
                # Cancel existing pending reconnect task if any
                if self._reconnect_task and not self._reconnect_task.done():
                    self._reconnect_task.cancel()
                self._reconnect_task = asyncio.create_task(
                    self._handle_reconnect()
                )

                def _clear_task(_):
                    self._reconnect_task = None

                self._reconnect_task.add_done_callback(_clear_task)
            except Exception:
                ...

    def _map_to_module_error_code(self, code: int, message: str) -> int:
        """Map Google Speech v2 / gRPC / HTTP error to fatal vs non-fatal.

        Priority:
        1) Numeric code (prefer gRPC canonical codes; also handle common HTTP codes)
        2) Text keywords fallback
        3) Default to non-fatal (safer)

        CANCELLED is already handled upstream as normal shutdown; if it reaches here, treat as non-fatal.

        see: https://cloud.google.com/speech-to-text/v2/docs/reference/rest/v2/Code
        """
        text = (message or "").upper()

        # gRPC canonical codes
        # Retryable (non-fatal)
        grpc_retryable = {
            1,
            2,
            4,
            8,
            10,
            13,
            14,
        }  # CANCELLED, UNKNOWN, DEADLINE_EXCEEDED, RESOURCE_EXHAUSTED, ABORTED, INTERNAL, UNAVAILABLE
        # Fatal (non-retryable)
        grpc_fatal = {
            3,
            5,
            6,
            7,
            9,
            11,
            12,
            15,
            16,
        }  # INVALID_ARGUMENT, NOT_FOUND, ALREADY_EXISTS, PERMISSION_DENIED, FAILED_PRECONDITION, OUT_OF_RANGE, UNIMPLEMENTED, DATA_LOSS, UNAUTHENTICATED

        # HTTP mappings commonly seen
        http_retryable = {429, 503, 504}
        http_fatal = {400, 401, 403, 404, 501}

        if code in grpc_retryable or code in http_retryable:
            return ModuleErrorCode.NON_FATAL_ERROR.value
        if code in grpc_fatal or code in http_fatal:
            return ModuleErrorCode.FATAL_ERROR.value

        # Keyword fallbacks
        retryable_hints = [
            "UNAVAILABLE",
            "DEADLINE_EXCEEDED",
            "RESOURCE_EXHAUSTED",
            "INTERNAL",
            "ABORTED",
            "RETRY",
            "TIMEOUT",
            "TEMPORARY",
        ]
        if any(h in text for h in retryable_hints):
            return ModuleErrorCode.NON_FATAL_ERROR.value

        if "CANCELLED" in text:
            return ModuleErrorCode.NON_FATAL_ERROR.value

        fatal_hints = [
            "UNSUPPORTED",
            "NOT SUPPORTED",
            "INVALID",
            "VALIDATION",
            "PERMISSION_DENIED",
            "UNAUTHENTICATED",
            "NOT_FOUND",
            "UNIMPLEMENTED",
            "DATA_LOSS",
            "OUT_OF_RANGE",
            "FAILED_PRECONDITION",
            "ALREADY_EXISTS",
        ]
        if any(h in text for h in fatal_hints):
            return ModuleErrorCode.FATAL_ERROR.value

        # Default non-fatal
        return ModuleErrorCode.NON_FATAL_ERROR.value

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Loads the configuration from property.json."""
        await super().on_init(ten_env)
        try:
            config_json, _ = await ten_env.get_property_to_json()
            self.config = GoogleASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)

            if self.config.dump:
                dump_dir = self.config.dump_path or "."
                os.makedirs(dump_dir, exist_ok=True)
                dump_file_path = os.path.join(dump_dir, "google_asr_in.pcm")
                self.audio_dumper = Dumper(dump_file_path)

            # Initialize reconnect manager
            self.reconnect_manager = ReconnectManager(logger=ten_env)
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
        except Exception as e:
            ten_env.log_error(f"Error during Google ASR initialization: {e}")
            self.config = GoogleASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=ModuleType.ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        """Starts the connection to Google ASR."""
        if self.connected:
            self.ten_env.log_warn("Connection already started.")
            return

        if not self.config or not self.ten_env:
            msg = "Extension not initialized properly. Config or ten_env is missing."
            raise RuntimeError(msg)

        self.ten_env.log_info("Starting Google ASR connection...")
        self.stopped = False
        try:
            # Start audio dumper as early as possible so tests that only verify dumping succeed
            if self.audio_dumper:
                await self.audio_dumper.start()

            self.client = GoogleASRClient(
                config=self.config,
                ten_env=self.ten_env,
                on_result_callback=self._on_asr_result,
                on_error_callback=self._on_asr_error,
            )
            await self.client.start(self.session_id)
            self.connected = True
            self.ten_env.log_info("Google ASR connection started successfully.")
            if self.reconnect_manager:
                self.reconnect_manager.mark_connection_successful()
        except Exception as e:
            self.ten_env.log_error(
                f"KEYPOINT Failed to start Google ASR connection: {e}"
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
            await self.client.stop(self.session_id)
        self.client = None
        self.connected = False
        if self.audio_dumper:
            await self.audio_dumper.stop()
        self.ten_env.log_info("Google ASR connection stopped.")
        self.stopped = True
        # Cancel any pending reconnect
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()

    @override
    def is_connected(self) -> bool:
        """Checks the connection status."""
        return self.connected and self.client is not None

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """Sends an audio frame for recognition."""
        if self.session_id != session_id:
            self.session_id = session_id

        buf = frame.lock_buf()
        try:
            # Always dump audio if enabled, even if vendor client is not connected
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(bytes(buf))

            # Only forward to vendor client when connected
            if self.is_connected() and self.client:
                self.audio_timeline.add_user_audio(
                    int(len(buf) / (self.config.sample_rate / 1000 * 2))
                )
                await self.client.send_audio(bytes(buf))
            else:
                self.ten_env.log_warn(
                    "Client not connected; audio dumped only."
                )
        finally:
            frame.unlock_buf(buf)

        return True

    @override
    async def finalize(self, session_id: str | None) -> None:
        """Finalizes the recognition for the current utterance."""
        if not self.is_connected() or not self.client:
            self.ten_env.log_warn("Cannot finalize, client not connected.")
            return

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        _ = self.ten_env.log_debug(
            f"KEYPOINT finalize start at {self.last_finalize_timestamp}]"
        )

        self.ten_env.log_info(f"Finalizing ASR for session: {session_id}")
        await self.client.finalize()
        await self.send_asr_finalize_end()

    async def _handle_reconnect(self) -> None:
        if not self.reconnect_manager:
            return
        # Avoid redundant attempts if already connected
        if self.connected or self.stopped:
            return
        try:
            success = await self.reconnect_manager.handle_reconnect(
                connection_func=self.start_connection,
                error_handler=None,
            )
            if success:
                self.ten_env.log_debug("Google ASR reconnect attempt completed")
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(
                    f"Reconnect attempt failed with exception: {e}"
                )

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        # Ensure we don't try to reconnect after deinit
        self.stopped = True
        if self._reconnect_task and not self._reconnect_task.done():
            self._reconnect_task.cancel()
        await super().on_deinit(ten_env)

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
