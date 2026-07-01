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
import base64
from typing import AsyncIterator

import requests
from text_utils.segmenter import SentenceSegmenter
from tn.chinese.normalizer import Normalizer as ZhNormalizer
import opencc
import wave, io
import soundfile

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
from .config import EZAITWTTSConfig

converter = opencc.OpenCC("s2t.json")
zh_tn_model = ZhNormalizer()
segmenter = SentenceSegmenter(token_limits=15)


class EZAITWTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: EZAITWTTSConfig | None = None
        self.current_request_id: str | None = None
        self.sent_ts: datetime | None = None
        self.total_audio_bytes: int = 0
        self.current_request_finished: bool = False
        self.recorder_map: dict[str, PCMWriter] = {}
        self._cancel_event: asyncio.Event | None = None
        self._request_lock = asyncio.Lock()
        self._text_buffers: dict[str, list[str]] = {}

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        try:
            await super().on_init(ten_env)
            config_json_str, _ = await self.ten_env.get_property_to_json("")

            if not config_json_str or config_json_str.strip() == "{}":
                raise ValueError("Configuration is empty.")

            self.config = EZAITWTTSConfig.model_validate_json(config_json_str)
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
        return "ezai"

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

    async def cancel_tts(self) -> None:
        if self._cancel_event is not None:
            self._cancel_event.set()

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
                    message="Empty text input for EZAI-TW TTS",
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

            base_payload = {
                "text": text,
                "language": "zh",
                "b64enc": True,
                "tw_convert": True,
                "autosplit": False,
                "speed": getattr(self.config, "speed", 0.8),
                "denoise": getattr(self.config, "denoise", False),
                "speaker": self.config.voice if self.config.voice else "",
                "zh_model": getattr(self.config, "zh_model", ""),
                # "zh_model": "nllb"
            }

            async def segment_text_and_tts(
                text: str,
            ) -> AsyncIterator[tuple[bytes, dict]]:
                text = zh_tn_model.normalize(text)
                text = converter.convert(text)

                sentences = [
                    st for t in text.split("\n") for st in segmenter.segment(t)
                ]

                payload = base_payload.copy()
                for sent in sentences:
                    payload["text"] = sent

                    resp = await asyncio.to_thread(
                        requests.post,
                        self.config.url,
                        headers={"Content-Type": "application/json"},
                        data=json.dumps(payload),
                        timeout=60,
                    )

                    if resp.status_code != 200:
                        raise RuntimeError(
                            f"TTS HTTP error: {resp.status_code}"
                        )

                    j = resp.json()

                    if "audio" not in j:
                        raise RuntimeError("No audio returned from TTS service")

                    audio_bytes = base64.b64decode(j["audio"])
                    pcm24io = io.BytesIO(audio_bytes)
                    pcm24io.name = "pcm24.wav"
                    data, samplerate = soundfile.read(pcm24io)
                    newio = io.BytesIO()
                    newio.name = "file16.wav"
                    soundfile.write(newio, data, samplerate, subtype="PCM_16")
                    newio.seek(0)

                    self.ten_env.log_info(
                        f"tts input:|{sent}| output:{j['text']}"
                    )
                    with wave.open(newio) as w:
                        n = w.getnframes()
                        frames = w.readframes(n)
                        info = {
                            "input_text": sent,
                            "output_text": j["text"],
                            "len": n,
                        }
                        yield frames, info

            error_msg = None
            info = None
            try:
                first = True
                ttfb_start = datetime.now()
                async for frames, info in segment_text_and_tts(text):
                    if self._cancel_event.is_set():
                        break
                    if first:
                        first = False
                        ttfb_ms = int(
                            (datetime.now() - ttfb_start).total_seconds() * 1000
                        )

                        await self.send_tts_audio_start(request_id=t.request_id)
                        await self.send_tts_ttfb_metrics(
                            request_id=t.request_id,
                            ttfb_ms=ttfb_ms,
                            extra_metadata={"voice": self.config.voice or ""},
                        )

                    if self.config.dump and t.request_id in self.recorder_map:
                        asyncio.create_task(
                            self.recorder_map[t.request_id].write(frames)
                        )
                    self.total_audio_bytes += len(frames)
                    await self.send_tts_audio_data(frames)

            except Exception:
                error_msg = (
                    f"TTS request failed: {traceback.format_exc()}, {info}"
                )
            finally:
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
