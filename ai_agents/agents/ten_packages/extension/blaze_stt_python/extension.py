#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import copy
import os
from datetime import datetime
from typing import Any

from typing_extensions import override

from ten_ai_base.asr import (
    ASRBufferConfig,
    ASRBufferConfigModeKeep,
    ASRResult,
    AsyncASRBaseExtension,
)
from ten_ai_base.const import (
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from ten_runtime import AsyncTenEnv, AudioFrame

from .config import BlazeSTTConfig
from .const import (
    AUDIO_BUFFER_BYTE_LIMIT,
    DUMP_FILE_NAME,
    MODULE_NAME_ASR,
    RECONNECT_MAX_ATTEMPTS,
)
from .recognition import (
    BlazeSTTAuthError,
    BlazeSTTRecognition,
    BlazeSTTRecognitionCallback,
)
from .reconnect_manager import ReconnectManager

FATAL_ERROR_CODES = {"401", "403", "failed-authentication"}


class BlazeSTTExtension(AsyncASRBaseExtension, BlazeSTTRecognitionCallback):
    """Blaze realtime STT over WebSocket."""

    def __init__(self, name: str):
        super().__init__(name)
        self.recognition: BlazeSTTRecognition | None = None
        self.config: BlazeSTTConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.reconnect_manager: ReconnectManager | None = None
        self.sent_user_audio_duration_ms_before_last_reset = 0
        self.last_finalize_timestamp = 0
        self.connection_start_timestamp = 0
        self._last_final_end_ms = 0
        self._has_pending_speech = False
        self._stop_requested = False
        self._replacing = False
        self._init_failed = False

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    def vendor(self) -> str:
        return "blaze"

    @override
    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        params = self.config.params or {}
        fields = {
            "key": self.config.api_key,
            "url": params.get("base_url"),
            "model": params.get("model"),
            "language": params.get("language"),
        }
        return {k: v for k, v in fields.items() if v}

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.reconnect_manager = ReconnectManager(
            logger=ten_env, max_attempts=RECONNECT_MAX_ATTEMPTS
        )
        config_json, _ = await ten_env.get_property_to_json("")
        try:
            self.config = BlazeSTTConfig.model_validate_json(config_json)
            self.config.apply_defaults()
            self.config.validate_config()
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
        except Exception as e:  # pylint: disable=broad-exception-caught
            ten_env.log_error(f"Invalid Blaze STT config: {e}")
            # Latch the failure so auto-connect does not run with the
            # fallback config below; one fatal error is reported here.
            self._init_failed = True
            self.config = BlazeSTTConfig.model_validate_json("{}")
            self.config.apply_defaults()
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    @override
    async def start_connection(self) -> None:
        if self._init_failed:
            return
        self._stop_requested = False
        try:
            await self._connect_recognition()
        except Exception as e:  # pylint: disable=broad-exception-caught
            fatal = isinstance(e, BlazeSTTAuthError)
            self.ten_env.log_error(
                f"vendor_error: Blaze STT connect failed: {e}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=(
                        ModuleErrorCode.FATAL_ERROR.value
                        if fatal
                        else ModuleErrorCode.NON_FATAL_ERROR.value
                    ),
                    message=str(e),
                ),
                ModuleErrorVendorInfo(
                    vendor=self.vendor(),
                    code=(
                        str(BlazeSTTAuthError.status_code)
                        if fatal
                        else "connect_failed"
                    ),
                    message=str(e),
                ),
            )
            if not fatal:
                await self._handle_reconnect()

    async def _connect_recognition(self) -> None:
        assert self.config is not None
        if self.recognition is not None:
            old = self.recognition
            self.recognition = None
            # The old socket's on_close must not trigger another reconnect.
            self._replacing = True
            try:
                await old.close()
            finally:
                self._replacing = False
        self.connection_start_timestamp = int(datetime.now().timestamp() * 1000)
        self.recognition = BlazeSTTRecognition(
            config=self.config,
            audio_timeline=self.audio_timeline,
            ten_env=self.ten_env,
            callback=self,
        )
        await self.recognition.start()

    @override
    async def finalize(self, _session_id: str | None) -> None:
        assert self.config is not None
        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        recognition = self.recognition
        if (
            recognition is None
            or not recognition.is_connected()
            or not self._has_pending_speech
        ):
            # Nothing in flight: Blaze has already emitted every final.
            await self._finalize_end()
            return
        try:
            recognition.final_event.clear()
            await recognition.send_silence(self.config.finalize_silence_ms)
            got_final = await recognition.wait_for_final(
                self.config.finalize_timeout_ms
            )
            if not got_final:
                self.ten_env.log_warn(
                    "asr_finalize: no final result before timeout",
                    category=LOG_CATEGORY_KEY_POINT,
                )
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_warn(
                f"asr_finalize: failed: {e}", category=LOG_CATEGORY_KEY_POINT
            )
        finally:
            await self._finalize_end()

    async def _finalize_end(self) -> None:
        if self.last_finalize_timestamp != 0:
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    @override
    async def stop_connection(self) -> None:
        self._stop_requested = True
        if self.recognition:
            recognition = self.recognition
            self.recognition = None
            await recognition.close()

    @override
    def is_connected(self) -> bool:
        return self.recognition is not None and self.recognition.is_connected()

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=AUDIO_BUFFER_BYTE_LIMIT)

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    @override
    async def send_audio(
        self, frame: AudioFrame, _session_id: str | None
    ) -> bool:
        if self.recognition is None or not self.is_connected():
            # Returning False lets the base class buffer the frame.
            return False
        buf = frame.lock_buf()
        try:
            audio_data = bytes(buf)
        finally:
            frame.unlock_buf(buf)
        try:
            if self.audio_dumper:
                await self.audio_dumper.push_bytes(audio_data)
            await self.recognition.send_audio_frame(audio_data)
            return True
        except Exception as e:  # pylint: disable=broad-exception-caught
            self.ten_env.log_error(f"Error sending audio to Blaze STT: {e}")
            return False

    # Vendor callbacks

    @override
    async def on_open(self) -> None:
        connection_delay_ms = (
            int(datetime.now().timestamp() * 1000)
            - self.connection_start_timestamp
        )
        self.ten_env.log_info(
            "vendor_status_changed: on_open", category=LOG_CATEGORY_VENDOR
        )
        await self.send_connect_delay_metrics(connection_delay_ms)
        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        self._last_final_end_ms = 0
        self._has_pending_speech = False

    def _actual_ms(self, vendor_ms: int) -> int:
        """Map a vendor stream timestamp to the session's user-audio time."""
        return int(
            self.audio_timeline.get_audio_duration_before_time(vendor_ms)
            + self.sent_user_audio_duration_ms_before_last_reset
        )

    def _build_metadata(self) -> dict[str, Any]:
        base_metadata = (
            copy.deepcopy(self.metadata) if self.metadata is not None else {}
        )
        session_id = base_metadata.pop("session_id", None)
        metadata: dict[str, Any] = {}
        if session_id is not None:
            metadata["session_id"] = session_id
        metadata["asr_info"] = {
            **base_metadata,
            "vendor": self.vendor(),
            "locked": False,
        }
        return metadata

    async def _emit_asr_result(
        self, text: str, final: bool, start_ms: int, duration_ms: int
    ) -> None:
        assert self.config is not None
        await self.send_asr_result(
            ASRResult(
                text=text,
                final=final,
                start_ms=max(0, start_ms),
                duration_ms=max(0, duration_ms),
                language=self.config.asr_result_language,
                words=[],
                metadata=self._build_metadata(),
            )
        )

    @override
    async def on_partial_result(self, message_data: dict[str, Any]) -> None:
        text = str(message_data.get("text") or "").strip()
        if not text:
            return
        self._has_pending_speech = True
        await self._emit_asr_result(
            text,
            final=False,
            start_ms=self._actual_ms(self._last_final_end_ms),
            duration_ms=0,
        )

    @override
    async def on_final_result(self, message_data: dict[str, Any]) -> None:
        self._has_pending_speech = False
        text = str(message_data.get("text") or "").strip()
        start = int(message_data.get("start_ms") or self._last_final_end_ms)
        end = int(message_data.get("end_ms") or start)
        self._last_final_end_ms = max(end, self._last_final_end_ms)
        if not text:
            return
        actual_start = self._actual_ms(start)
        await self._emit_asr_result(
            text,
            final=True,
            start_ms=actual_start,
            duration_ms=self._actual_ms(end) - actual_start,
        )

    @override
    async def on_error(
        self, error_msg: str, error_code: int | str | None = None
    ) -> None:
        self.ten_env.log_error(
            f"vendor_error: code: {error_code}, reason: {error_msg}",
            category=LOG_CATEGORY_VENDOR,
        )
        code = str(error_code) if error_code is not None else "unknown"
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=(
                    ModuleErrorCode.FATAL_ERROR.value
                    if code in FATAL_ERROR_CODES
                    else ModuleErrorCode.NON_FATAL_ERROR.value
                ),
                message=error_msg,
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(), code=code, message=error_msg
            ),
        )

    @override
    async def on_close(self) -> None:
        self.ten_env.log_info(
            "vendor_status_changed: on_close", category=LOG_CATEGORY_VENDOR
        )
        self._has_pending_speech = False
        if self._stop_requested or self._replacing:
            return
        await self._handle_reconnect()

    async def _handle_reconnect(self) -> None:
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return
        while not self._stop_requested and self.reconnect_manager.can_retry():
            success = await self.reconnect_manager.handle_reconnect(
                connection_func=self._connect_recognition,
                error_handler=self.send_asr_error,
                vendor_name=self.vendor(),
                vendor_code="connect_failed",
            )
            if success:
                return
