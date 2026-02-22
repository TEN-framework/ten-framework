from datetime import datetime
import os
import asyncio
from typing import Dict, Any

from typing_extensions import override
from .const import (
    DUMP_FILE_NAME,
    MODULE_NAME_ASR,
)
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
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
)
from ten_ai_base.const import (
    LOG_CATEGORY_VENDOR,
    LOG_CATEGORY_KEY_POINT,
)

from ten_ai_base.dumper import Dumper
from .reconnect_manager import ReconnectManager
from .recognition import OracleASRRecognition, OracleASRRecognitionCallback
from .config import OracleASRConfig


class OracleASRExtension(
    AsyncASRBaseExtension, OracleASRRecognitionCallback
):
    """Oracle Cloud Infrastructure Speech Realtime ASR Extension"""

    def __init__(self, name: str):
        super().__init__(name)
        self.recognition: OracleASRRecognition | None = None
        self.config: OracleASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0
        self.reconnect_manager: ReconnectManager = None  # type: ignore
        self._reconnect_lock = asyncio.Lock()

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    def vendor(self) -> str:
        return "oracle"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = OracleASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
                await self.audio_dumper.start()
        except Exception as e:
            ten_env.log_error(
                f"Invalid Oracle ASR config: {e}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            self.config = OracleASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        assert self.config is not None
        self.ten_env.log_info(
            "Starting Oracle Speech connection",
            category=LOG_CATEGORY_VENDOR,
        )

        try:
            tenancy = self.config.params.get("tenancy", "")
            user = self.config.params.get("user", "")
            fingerprint = self.config.params.get("fingerprint", "")
            key_file = self.config.params.get("key_file", "")
            compartment_id = self.config.params.get("compartment_id", "")

            missing = []
            if not tenancy:
                missing.append("tenancy")
            if not user:
                missing.append("user")
            if not fingerprint:
                missing.append("fingerprint")
            if not key_file:
                missing.append("key_file")
            if not compartment_id:
                missing.append("compartment_id")

            if missing:
                error_msg = f"Oracle ASR credentials missing: {', '.join(missing)}"
                self.ten_env.log_error(
                    error_msg, category=LOG_CATEGORY_KEY_POINT
                )
                await self.send_asr_error(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=error_msg,
                    ),
                )
                return

            if self.is_connected():
                await self.stop_connection()

            self.recognition = OracleASRRecognition(
                ten_env=self.ten_env,
                audio_timeline=self.audio_timeline,
                config=self.config.params,
                callback=self,
            )
            await self.recognition.start(timeout=10)

        except Exception as e:
            self.ten_env.log_error(
                f"Failed to start Oracle Speech connection: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def finalize(self, _session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_debug(
            f"Oracle ASR finalize start at {self.last_finalize_timestamp}"
        )

        if self.recognition:
            await self.recognition.request_final_result()

    async def _handle_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int = 0,
        duration_ms: int = 0,
        language: str = "",
    ):
        assert self.config is not None

        if final:
            await self._finalize_end()

        asr_result = ASRResult(
            text=text,
            final=final,
            start_ms=start_ms,
            duration_ms=duration_ms,
            language=language,
            words=[],
        )

        await self.send_asr_result(asr_result)

    async def _handle_reconnect(self):
        if not self.reconnect_manager:
            self.ten_env.log_error(
                "ReconnectManager not initialized",
                category=LOG_CATEGORY_KEY_POINT,
            )
            return

        if self._reconnect_lock.locked():
            self.ten_env.log_debug(
                "Reconnect already in progress, skip duplicate trigger",
                category=LOG_CATEGORY_VENDOR,
            )
            return

        async with self._reconnect_lock:
            success = await self.reconnect_manager.handle_reconnect(
                connection_func=self.start_connection,
                error_handler=self.send_asr_error,
            )

            if success:
                self.ten_env.log_debug(
                    "Reconnection attempt initiated successfully",
                    category=LOG_CATEGORY_VENDOR,
                )
            else:
                info = self.reconnect_manager.get_attempts_info()
                self.ten_env.log_debug(
                    f"Reconnection attempt failed. Status: {info}",
                    category=LOG_CATEGORY_VENDOR,
                )

    async def _finalize_end(self) -> None:
        if self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"Oracle ASR finalize end at {timestamp}, latency: {latency}ms"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def stop_connection(self) -> None:
        self.ten_env.log_info(
            "Stopping Oracle Speech connection",
            category=LOG_CATEGORY_VENDOR,
        )
        try:
            if self.recognition:
                await self.recognition.close()
                self.recognition = None
            self.ten_env.log_info(
                "Oracle Speech connection stopped",
                category=LOG_CATEGORY_VENDOR,
            )
        except Exception as e:
            self.ten_env.log_error(
                f"Error stopping Oracle Speech connection: {e}",
                category=LOG_CATEGORY_VENDOR,
            )

    @override
    def is_connected(self) -> bool:
        return self.recognition is not None and self.recognition.is_connected()

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return int(self.config.params.get("sample_rate", 16000))

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        assert self.recognition is not None

        buf = None
        try:
            buf = frame.lock_buf()
            audio_data = bytes(buf)

            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)

            await self.recognition.send_audio_frame(audio_data)
            return True

        except Exception as e:
            self.ten_env.log_error(
                f"Error sending audio to Oracle Speech: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            return False
        finally:
            if buf is not None:
                frame.unlock_buf(buf)

    # --- Vendor callback implementations ---

    @override
    async def on_open(self) -> None:
        self.ten_env.log_info(
            "vendor_status_changed: on_open",
            category=LOG_CATEGORY_VENDOR,
        )
        self.reconnect_manager.mark_connection_successful()

        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()

    @override
    async def on_result(self, message_data: Dict[str, Any]) -> None:
        try:
            transcriptions = message_data.get("transcriptions", [])
            if not transcriptions:
                self.ten_env.log_debug(
                    "No transcriptions in Oracle result",
                    category=LOG_CATEGORY_VENDOR,
                )
                return

            first = transcriptions[0]
            text = first.get("transcription", "").strip()
            is_final = first.get("isFinal", False)

            start_ms = int(first.get("startTimeMs", 0))
            end_ms = int(first.get("endTimeMs", 0))
            duration_ms = end_ms - start_ms if end_ms > start_ms else 0

            actual_start_ms = int(
                self.audio_timeline.get_audio_duration_before_time(start_ms)
                + self.sent_user_audio_duration_ms_before_last_reset
            )

            await self._handle_asr_result(
                text=text,
                final=is_final,
                start_ms=actual_start_ms,
                duration_ms=duration_ms,
                language=self.config.normalized_language,
            )

        except Exception as e:
            self.ten_env.log_error(
                f"Error processing Oracle result: {e}",
                category=LOG_CATEGORY_VENDOR,
            )

    @override
    async def on_error(
        self, error_msg: str, error_code: int | None = None
    ) -> None:
        self.ten_env.log_error(
            f"vendor_error: code: {error_code}, reason: {error_msg}",
            category=LOG_CATEGORY_VENDOR,
        )

        fatal_indicators = ["401", "403", "InvalidParameter", "AuthFail"]
        if any(ind in str(error_msg) for ind in fatal_indicators):
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=error_msg,
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code=str(error_code) if error_code else "unknown",
                    message=error_msg,
                ),
            )
        else:
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=error_msg,
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code=str(error_code) if error_code else "unknown",
                    message=error_msg,
                ),
            )

            if not self.stopped and not self.is_connected():
                self.ten_env.log_warn(
                    "Oracle Speech connection error. Reconnecting..."
                )
                await self._handle_reconnect()

    @override
    async def on_close(self) -> None:
        self.ten_env.log_info(
            "vendor_status_changed: on_close",
            category=LOG_CATEGORY_VENDOR,
        )

        if not self.stopped:
            self.ten_env.log_warn(
                "Oracle Speech connection closed unexpectedly. Reconnecting..."
            )
            await self._handle_reconnect()
