#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from datetime import datetime
import json
import os
import traceback
from typing import Optional
from urllib.parse import parse_qsl, urlencode, urlparse, urlunparse

import websockets

from ten_ai_base.const import LOG_CATEGORY_KEY_POINT
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

from .config import VibeVoiceTTSConfig


class VibeVoiceTTSWebsocketExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: VibeVoiceTTSConfig | None = None
        self.current_request_id: str | None = None
        self.sent_ts: datetime | None = None
        self.total_audio_bytes: int = 0
        self.current_request_finished: bool = False
        self.recorder_map: dict[str, PCMWriter] = {}
        self._active_ws: websockets.ClientConnection | None = None
        self._cancel_event: asyncio.Event | None = None
        self._request_lock = asyncio.Lock()
        self._text_buffers: dict[str, list[str]] = {}

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError("Configuration is empty.")

            self.config = VibeVoiceTTSConfig.model_validate_json(
                config_json_str
            )
            self.config.update_params()

            ten_env.log_info(
                f"LOG_CATEGORY_KEY_POINT: {self.config.to_str(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
        except Exception as exc:
            ten_env.log_error(f"on_init failed: {traceback.format_exc()}")
            await self.send_tts_error(
                request_id="",
                error=ModuleError(
                    message=f"Initialization failed: {exc}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self._active_ws is not None:
            try:
                await self._active_ws.close()
            except Exception:
                pass
            self._active_ws = None

        for request_id, recorder in list(self.recorder_map.items()):
            try:
                await recorder.flush()
                ten_env.log_debug(
                    f"Flushed PCMWriter for request_id: {request_id}"
                )
            except Exception as exc:
                ten_env.log_error(
                    f"Error flushing PCMWriter for request_id {request_id}: {exc}"
                )

        await super().on_stop(ten_env)
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        ten_env.log_debug("on_deinit")

    def vendor(self) -> str:
        return "vibevoice"

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate if self.config else 24000

    def synthesize_audio_channels(self) -> int:
        return self.config.channels if self.config else 1

    def synthesize_audio_sample_width(self) -> int:
        return self.config.sample_width if self.config else 2

    def _calculate_audio_duration_ms(self) -> int:
        if self.config is None:
            return 0
        bytes_per_sample = self.synthesize_audio_sample_width()
        channels = self.synthesize_audio_channels()
        duration_sec = self.total_audio_bytes / (
            self.synthesize_audio_sample_rate() * bytes_per_sample * channels
        )
        return int(duration_sec * 1000)

    def _build_ws_url(self, text: str) -> str:
        base_url = self.config.url
        parsed = urlparse(base_url)
        query = dict(parse_qsl(parsed.query))
        query["text"] = text
        query["cfg"] = str(self.config.cfg_scale)
        if self.config.steps is not None:
            query["steps"] = str(self.config.steps)
        if self.config.voice:
            query["voice"] = self.config.voice
        new_query = urlencode(query)
        return urlunparse(parsed._replace(query=new_query))

    async def cancel_tts(self) -> None:
        if self._cancel_event is not None:
            self._cancel_event.set()

        if self._active_ws is not None:
            try:
                await self._active_ws.close()
            except Exception:
                pass
            self._active_ws = None

        if self.current_request_id and self.sent_ts:
            request_event_interval = int(
                (datetime.now() - self.sent_ts).total_seconds() * 1000
            )
            duration_ms = self._calculate_audio_duration_ms()
            await self.send_tts_audio_end(
                request_id=self.current_request_id,
                request_event_interval_ms=request_event_interval,
                request_total_audio_duration_ms=duration_ms,
                reason=TTSAudioEndReason.INTERRUPTED,
            )
            await self.send_usage_metrics(self.current_request_id)
            self.sent_ts = None
            self.total_audio_bytes = 0
            self.current_request_finished = True

    async def request_tts(self, t: TTSTextInput) -> None:
        if self.config is None:
            await self.send_tts_error(
                t.request_id,
                ModuleError(
                    message="TTS extension not initialized",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
            return

        buffer = self._text_buffers.setdefault(t.request_id, [])
        if t.text:
            buffer.append(t.text)

        if not t.text_input_end:
            return

        text = "".join(buffer)
        self._text_buffers.pop(t.request_id, None)

        if not text.strip():
            await self.send_tts_error(
                t.request_id,
                ModuleError(
                    message="Empty text input for VibeVoice TTS",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor()),
                ),
            )
            await self.send_tts_audio_end(
                request_id=t.request_id,
                request_event_interval_ms=0,
                request_total_audio_duration_ms=0,
                reason=TTSAudioEndReason.ERROR,
            )
            return

        async with self._request_lock:
            self.current_request_id = t.request_id
            self.current_request_finished = False
            self.total_audio_bytes = 0
            self.sent_ts = datetime.now()
            self._cancel_event = asyncio.Event()

            if self.config.dump and t.request_id not in self.recorder_map:
                dump_file_path = os.path.join(
                    self.config.dump_path,
                    f"vibevoice_dump_{t.request_id}.pcm",
                )
                self.recorder_map[t.request_id] = PCMWriter(dump_file_path)

            ws_url = self._build_ws_url(text)
            first_chunk = True
            ttfb_start = datetime.now()
            error_msg: Optional[str] = None

            try:
                self._active_ws = await websockets.connect(
                    ws_url, max_size=1024 * 1024 * 16
                )

                while not self._cancel_event.is_set():
                    msg = await self._active_ws.recv()
                    if isinstance(msg, bytes):
                        if first_chunk:
                            first_chunk = False
                            ttfb_ms = int(
                                (datetime.now() - ttfb_start).total_seconds()
                                * 1000
                            )
                            await self.send_tts_audio_start(
                                request_id=t.request_id
                            )
                            await self.send_tts_ttfb_metrics(
                                request_id=t.request_id,
                                ttfb_ms=ttfb_ms,
                                extra_metadata={
                                    "voice": self.config.voice or "",
                                    "steps": self.config.steps,
                                    "cfg_scale": self.config.cfg_scale,
                                },
                            )

                        if (
                            self.config.dump
                            and t.request_id in self.recorder_map
                        ):
                            asyncio.create_task(
                                self.recorder_map[t.request_id].write(msg)
                            )

                        self.total_audio_bytes += len(msg)
                        await self.send_tts_audio_data(msg)
                    else:
                        try:
                            payload = json.loads(msg)
                        except json.JSONDecodeError:
                            continue
                        if payload.get("event") == "backend_busy":
                            error_msg = payload.get("data", {}).get(
                                "message", "VibeVoice backend busy"
                            )
                            break
                        if payload.get("event") == "generation_error":
                            error_msg = payload.get("data", {}).get(
                                "message", "VibeVoice generation error"
                            )
                            break
            except websockets.ConnectionClosedOK:
                pass
            except websockets.ConnectionClosedError as exc:
                error_msg = f"Websocket closed with error: {exc}"
            except Exception as exc:
                error_msg = f"Unexpected error: {exc}"
            finally:
                if self._active_ws is not None:
                    try:
                        await self._active_ws.close()
                    except Exception:
                        pass
                self._active_ws = None

                if self.config.dump and t.request_id in self.recorder_map:
                    try:
                        await self.recorder_map[t.request_id].flush()
                    except Exception:
                        pass

                if error_msg:
                    await self.send_tts_error(
                        request_id=t.request_id,
                        error=ModuleError(
                            message=error_msg,
                            module=ModuleType.TTS,
                            code=ModuleErrorCode.NON_FATAL_ERROR,
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor()
                            ),
                        ),
                    )
                    await self.send_tts_audio_end(
                        request_id=t.request_id,
                        request_event_interval_ms=0,
                        request_total_audio_duration_ms=0,
                        reason=TTSAudioEndReason.ERROR,
                    )
                else:
                    request_event_interval = 0
                    if self.sent_ts is not None:
                        request_event_interval = int(
                            (datetime.now() - self.sent_ts).total_seconds()
                            * 1000
                        )
                    duration_ms = self._calculate_audio_duration_ms()
                    await self.send_tts_audio_end(
                        request_id=t.request_id,
                        request_event_interval_ms=request_event_interval,
                        request_total_audio_duration_ms=duration_ms,
                        reason=TTSAudioEndReason.REQUEST_END,
                    )
                    await self.send_usage_metrics(t.request_id)

                self.sent_ts = None
                self.current_request_finished = True
                self.total_audio_bytes = 0
