#
# Breeze TTS extension for the TEN Framework.
# Licensed under the Apache License, Version 2.0.
#
from __future__ import annotations

import asyncio
import time
import traceback
from pathlib import Path

from typing_extensions import override

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT, LOG_CATEGORY_VENDOR
from ten_ai_base.dumper import Dumper
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    TTSAudioEndReason,
)
from ten_ai_base.struct import TTSTextInput
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from ten_runtime import AsyncTenEnv

try:
    from .breeze_tts import BreezeTTS
    from .config import BreezeTTSConfig
except ImportError:  # running outside the TEN package
    from breeze_tts import BreezeTTS
    from config import BreezeTTSConfig


class BreezeTTSExtension(AsyncTTS2BaseExtension):
    """Stream Breeze PCM audio into the TEN pipeline."""

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: BreezeTTSConfig | None = None
        self.client: BreezeTTS | None = None
        self.request_task: asyncio.Task | None = None
        self.audio_dumper: dict[str, Dumper] | None = None

    @override
    def vendor(self) -> str:
        return "breeze"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        config_json, _ = await ten_env.get_property_to_json("")
        try:
            self.config = BreezeTTSConfig.model_validate_json(config_json)
            ten_env.log_info(
                "breeze_tts config loaded",
                category=LOG_CATEGORY_KEY_POINT,
            )
            if self.config.dump:
                self.audio_dumper = {}
            self.client = BreezeTTS(self.config.params)
            await self.client.start()
        except Exception as error:
            ten_env.log_error(
                f"vendor_status: tts on_init error: {error}",
                category=LOG_CATEGORY_VENDOR,
            )
            self.config = None
            self.client = None
            raise

    @override
    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.request_task is not None:
            self.request_task.cancel()
            self.request_task = None
        if self.client is not None:
            await self.client.stop()
        await super().on_stop(ten_env)

    async def cancel_tts(self) -> None:
        if self.request_task is not None:
            self.request_task.cancel()
            self.request_task = None

    @override
    async def request_tts(self, t: TTSTextInput) -> None:
        if self.client is None or self.config is None:
            self.ten_env.log_error(
                "vendor_status: tts client is not ready",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_tts_error(
                t.request_id,
                ModuleError(
                    message="Breeze TTS client is not initialized",
                    module="tts",
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(vendor="breeze"),
                ),
            )
            return
        if not t.text.strip():
            return
        self.request_task = asyncio.create_task(self._async_synthesize(t))
        try:
            await self.request_task
        except asyncio.CancelledError:
            pass
        finally:
            self.request_task = None

    @override
    def synthesize_audio_sample_rate(self) -> int:
        if self.config is None:
            return 24000
        return int(self.config.params.sample_rate)

    async def _async_synthesize(self, text_input: TTSTextInput) -> None:
        assert self.client is not None
        request_id = text_input.request_id
        sample_rate = self.synthesize_audio_sample_rate()
        started = False
        total_duration_ms = 0
        request_start = time.time()

        try:
            async for chunk in self.client.synthesize(text_input.text):
                if not started:
                    started = True
                    await self.send_tts_audio_start(request_id)
                    await self.send_tts_ttfb_metrics(
                        request_id=request_id,
                        ttfb_ms=int((time.time() - request_start) * 1000),
                    )
                total_duration_ms += int(len(chunk) * 1000 / (sample_rate * 2))
                await self.send_tts_audio_data(chunk)
                if self.audio_dumper is not None:
                    await self._dump(request_id, chunk)
        except asyncio.CancelledError:
            if started:
                await self.send_tts_audio_end(
                    request_id,
                    int((time.time() - request_start) * 1000),
                    total_duration_ms,
                    reason=TTSAudioEndReason.INTERRUPTED,
                )
            return
        except Exception as error:
            self.ten_env.log_error(
                f"vendor_status: {traceback.format_exc()}",
                category=LOG_CATEGORY_VENDOR,
            )
            await self.send_tts_error(
                request_id,
                ModuleError(
                    message=str(error),
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    vendor_info=ModuleErrorVendorInfo(
                        vendor="breeze", message=str(error)
                    ),
                ),
            )
            if started:
                await self.send_tts_audio_end(
                    request_id,
                    int((time.time() - request_start) * 1000),
                    total_duration_ms,
                    reason=TTSAudioEndReason.ERROR,
                )
            return

        if started and text_input.text_input_end:
            await self.send_tts_audio_end(
                request_id,
                int((time.time() - request_start) * 1000),
                total_duration_ms,
                reason=TTSAudioEndReason.REQUEST_END,
            )

    async def _dump(self, request_id: str, chunk: bytes) -> None:
        assert self.config is not None
        assert self.audio_dumper is not None
        dumper = self.audio_dumper.get(request_id)
        if dumper is None:
            path = Path(self.config.dump_path)
            if path.suffix == "":
                path = path / f"breeze_tts_in_{request_id}.pcm"
            path.parent.mkdir(parents=True, exist_ok=True)
            dumper = Dumper(str(path))
            await dumper.start()
            self.audio_dumper[request_id] = dumper
        await dumper.push_bytes(chunk)
