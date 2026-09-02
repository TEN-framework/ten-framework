#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import os
import time
import traceback
from typing import Any

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_ai_base.helper import PCMWriter
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleType,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import AsyncTenEnv

from .client import (
    SpekoRouterError,
    SpekoTTSClient,
    SpekoTTSEventType,
)
from .config import SpekoTTS2Config


class SpekoTTS2Extension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: SpekoTTS2Config | None = None
        self.client: SpekoTTSClient | None = None
        self.current_request_id: str | None = None
        self._route: dict[str, Any] = {}
        self._router_usage: dict[str, Any] = {}
        self._audio_start_sent = False
        self._first_audio_at: float | None = None
        self._total_audio_bytes = 0
        self._finalized = False
        self._stopped = False
        self._recorders: dict[str, PCMWriter] = {}

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        try:
            config_json, error = await ten_env.get_property_to_json("")
            if error:
                raise RuntimeError(f"Failed to read configuration: {error}")
            self.config = SpekoTTS2Config.model_validate_json(config_json)
            self.config.update_params()
            ten_env.log_info(
                f"config: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
        except Exception as error:
            self.config = None
            ten_env.log_error(
                f"invalid property: {error}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(error),
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        self._stopped = True
        await self._close_client()
        for recorder in self._recorders.values():
            await recorder.flush()
        self._recorders.clear()
        await super().on_stop(ten_env)

    def vendor(self) -> str:
        return "speko"

    def vendor_metadata(self) -> dict[str, Any]:
        if self.config is None:
            return {}
        metadata: dict[str, Any] = {
            "base_url": self.config.base_url,
            "routing": self.config.routing,
        }
        if self._route:
            metadata["route"] = self._route
            metadata["model"] = self._route.get("model", "")
        return metadata

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 24000

    def synthesize_audio_channels(self) -> int:
        return self.config.channels if self.config else 1

    async def request_tts(self, text_input: TTSTextInput) -> None:
        if self.config is None or self._stopped:
            return

        try:
            if text_input.request_id != self.current_request_id:
                await self._begin_request(text_input)
            if self._finalized:
                self.ten_env.log_warn(
                    "Ignoring text for a completed Speko TTS request"
                )
                return

            text = text_input.text.strip()
            if text:
                await self._stream_text(text, text_input.request_id)

            if text_input.text_input_end:
                await self._finalize_request(TTSAudioEndReason.REQUEST_END)
        except SpekoRouterError as error:
            await self._handle_router_error(error)
        except Exception as error:
            self.ten_env.log_error(
                f"Speko TTS request failed: {traceback.format_exc()}"
            )
            await self.on_disconnected(
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=str(error),
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
            )
            await self._finalize_request(
                TTSAudioEndReason.ERROR,
                error=ModuleError(
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message=str(error),
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def cancel_tts(self) -> None:
        request_id = self.current_request_id
        if request_id is None or self._finalized:
            return
        self._finalized = True
        if self.client is not None:
            await self.client.cancel()
            self.client = None
        await self.on_disconnected(code=0, message="cancelled")
        await self._ensure_audio_start()
        await self.send_tts_audio_end(
            request_id=request_id,
            request_event_interval_ms=self._request_interval_ms(),
            request_total_audio_duration_ms=self._audio_duration_ms(),
            reason=TTSAudioEndReason.INTERRUPTED,
        )
        await self._flush_recorder(request_id)

    async def _begin_request(self, text_input: TTSTextInput) -> None:
        if self.client is not None:
            await self._close_client()
        self.current_request_id = text_input.request_id
        self._route = {}
        self._router_usage = {}
        self._audio_start_sent = False
        self._first_audio_at = None
        self._total_audio_bytes = 0
        self._finalized = False
        await self._setup_recorder(text_input.request_id)

    async def _ensure_client(self) -> SpekoTTSClient:
        if self.client is not None and self.client.is_ready:
            return self.client
        if self.config is None:
            raise RuntimeError("Speko TTS is not configured")

        await self.on_connecting()
        connected_at = time.monotonic()
        client = SpekoTTSClient(
            api_key=self.config.api_key,
            base_url=self.config.base_url,
            configure=self._configure_frame(),
            ready_timeout_sec=self.config.ready_timeout_sec,
            receive_timeout_sec=self.config.receive_timeout_sec,
        )
        await client.connect()
        self.client = client
        self._route = client.route
        await self.on_connected()
        await self.metrics_connect_delay(
            int((time.monotonic() - connected_at) * 1000),
            extra_metadata={"route": self._route},
            request_id=self.current_request_id or "",
        )
        self.ten_env.log_info(
            f"vendor_status_changed: Speko TTS ready; route={self._route}",
            category=LOG_CATEGORY_VENDOR,
        )
        return client

    async def _stream_text(self, text: str, request_id: str) -> None:
        client = await self._ensure_client()
        self.metrics_add_input_characters(len(text))
        async for event in client.stream_text(text):
            if event.type == SpekoTTSEventType.TTFB:
                if not self._audio_start_sent:
                    await self._ensure_audio_start()
                    await self.send_tts_ttfb_metrics(
                        request_id=request_id,
                        ttfb_ms=int(event.value or 0),
                        extra_metadata={"route": self._route},
                    )
            elif event.type == SpekoTTSEventType.AUDIO:
                audio = event.value
                if not isinstance(audio, bytes) or not audio:
                    continue
                await self._ensure_audio_start()
                if self._first_audio_at is None:
                    self._first_audio_at = time.monotonic()
                self._total_audio_bytes += len(audio)
                self.metrics_add_recv_audio_chunks(audio)
                await self._write_dump(request_id, audio)
                await self.send_tts_audio_data(audio)
            elif event.type == SpekoTTSEventType.USAGE:
                if isinstance(event.value, dict):
                    self._router_usage = event.value

    async def _finalize_request(
        self,
        reason: TTSAudioEndReason,
        error: ModuleError | None = None,
    ) -> None:
        request_id = self.current_request_id
        if request_id is None or self._finalized:
            return
        self._finalized = True
        await self._ensure_audio_start()
        await self.send_tts_audio_end(
            request_id=request_id,
            request_event_interval_ms=self._request_interval_ms(),
            request_total_audio_duration_ms=self._audio_duration_ms(),
            reason=reason,
            extra_metadata={"router_usage": self._router_usage},
        )
        await self._close_client()
        await self.send_usage_metrics(
            request_id,
            extra_metadata={"router_usage": self._router_usage},
        )
        await self._flush_recorder(request_id)
        await self.finish_request(request_id, reason=reason, error=error)

    async def _handle_router_error(self, error: SpekoRouterError) -> None:
        self.ten_env.log_error(
            f"vendor_error: code={error.code}, message={error.message}",
            category=LOG_CATEGORY_VENDOR,
        )
        module_code = self._module_error_code(error)
        module_error = ModuleError(
            module=ModuleType.TTS,
            code=module_code.value,
            message=error.message,
            vendor_info=ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=error.code,
                message=error.message,
            ),
        )
        await self.on_disconnected(
            code=module_code.value,
            message=error.message,
            vendor_info=module_error.vendor_info,
        )
        if self._finalized:
            await self.send_tts_error(self.current_request_id, module_error)
            return
        await self._finalize_request(
            TTSAudioEndReason.ERROR,
            error=module_error,
        )

    async def _ensure_audio_start(self) -> None:
        if self._audio_start_sent or self.current_request_id is None:
            return
        await self.send_tts_audio_start(
            request_id=self.current_request_id,
            extra_metadata={"route": self._route},
        )
        self._audio_start_sent = True

    async def _close_client(self) -> None:
        if self.client is None:
            return
        client = self.client
        try:
            await client.close()
        finally:
            self._router_usage = client.usage or self._router_usage
            self.client = None
        await self.on_disconnected(code=0, message="closed")

    def _configure_frame(self) -> dict[str, Any]:
        assert self.config is not None
        frame: dict[str, Any] = {
            "type": "session.configure",
            "audio": {
                "encoding": "pcm_s16le",
                "sample_rate_hz": self.config.sample_rate,
                "channels": self.config.channels,
            },
        }
        if self.config.routing:
            frame["routing"] = self.config.routing
        if self.config.language:
            frame["language"] = self.config.language
        if self.config.voice:
            frame["voice"] = self.config.voice
        return frame

    async def _setup_recorder(self, request_id: str) -> None:
        if self.config is None or not self.config.dump:
            return
        os.makedirs(self.config.dump_path, exist_ok=True)
        path = os.path.join(
            self.config.dump_path, f"speko_tts_{request_id}.pcm"
        )
        self._recorders[request_id] = PCMWriter(path)

    async def _write_dump(self, request_id: str, audio: bytes) -> None:
        recorder = self._recorders.get(request_id)
        if recorder is not None:
            await recorder.write(audio)

    async def _flush_recorder(self, request_id: str) -> None:
        recorder = self._recorders.pop(request_id, None)
        if recorder is not None:
            await recorder.flush()

    def _request_interval_ms(self) -> int:
        if self._first_audio_at is None:
            return 0
        return int((time.monotonic() - self._first_audio_at) * 1000)

    def _audio_duration_ms(self) -> int:
        bytes_per_second = (
            self.synthesize_audio_sample_rate()
            * self.synthesize_audio_channels()
            * self.synthesize_audio_sample_width()
        )
        return int(self._total_audio_bytes * 1000 / bytes_per_second)

    @staticmethod
    def _module_error_code(error: SpekoRouterError) -> ModuleErrorCode:
        fatal_codes = {"authentication_failed", "insufficient_credit"}
        if error.code in fatal_codes:
            return ModuleErrorCode.FATAL_ERROR
        return ModuleErrorCode.NON_FATAL_ERROR
