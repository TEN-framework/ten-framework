"""Deepgram WebSocket ASR Extension - Direct WebSocket implementation for Nova and Flux."""
from datetime import datetime
import json
import asyncio
from typing import Optional
from urllib.parse import urlencode

import aiohttp
from typing_extensions import override

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
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)

from .config import DeepgramWSASRConfig

MODULE_NAME_ASR = "deepgram_ws_asr"


class DeepgramWSASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.ws: Optional[aiohttp.ClientWebSocketResponse] = None
        self.session: Optional[aiohttp.ClientSession] = None
        self.config: Optional[DeepgramWSASRConfig] = None
        self.audio_frame_count: int = 0
        self.receive_task: Optional[asyncio.Task] = None

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        await self.stop_connection()

    @override
    def vendor(self) -> str:
        return "deepgram"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = DeepgramWSASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)

            ten_env.log_info("=" * 60)
            ten_env.log_info("[DEEPGRAM-WS] Deepgram WebSocket ASR Configuration")
            ten_env.log_info("=" * 60)
            ten_env.log_info(f"[DEEPGRAM-WS] Model: {self.config.model}")
            ten_env.log_info(f"[DEEPGRAM-WS] Language: {self.config.language}")
            ten_env.log_info(f"[DEEPGRAM-WS] URL: {self.config.url}")
            ten_env.log_info(f"[DEEPGRAM-WS] Sample Rate: {self.config.sample_rate}")
            ten_env.log_info(f"[DEEPGRAM-WS] Encoding: {self.config.encoding}")
            ten_env.log_info(f"[DEEPGRAM-WS] Interim Results: {self.config.interim_results}")
            ten_env.log_info(f"[DEEPGRAM-WS] Punctuate: {self.config.punctuate}")

            if self.config.is_v2_endpoint():
                ten_env.log_info(f"[DEEPGRAM-WS] API Version: v2 (Flux)")
                ten_env.log_info(f"[DEEPGRAM-WS] EOT Threshold: {self.config.eot_threshold}")
                ten_env.log_info(f"[DEEPGRAM-WS] EOT Timeout: {self.config.eot_timeout_ms}ms")
                if self.config.eager_eot_threshold > 0:
                    ten_env.log_info(f"[DEEPGRAM-WS] Eager EOT Threshold: {self.config.eager_eot_threshold}")
            else:
                ten_env.log_info(f"[DEEPGRAM-WS] API Version: v1 (Nova)")

            ten_env.log_info("=" * 60)

        except Exception as e:
            ten_env.log_error(f"Invalid property: {e}")
            self.config = DeepgramWSASRConfig.model_validate_json("{}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    def _build_websocket_url(self) -> str:
        assert self.config is not None

        if self.config.is_v2_endpoint():
            params = {
                "model": self.config.model,
                "sample_rate": self.config.sample_rate,
                "encoding": self.config.encoding,
            }
            if self.config.eot_threshold > 0:
                params["eot_threshold"] = str(self.config.eot_threshold)
            if self.config.eot_timeout_ms > 0:
                params["eot_timeout_ms"] = str(self.config.eot_timeout_ms)
            if self.config.eager_eot_threshold > 0:
                params["eager_eot_threshold"] = str(self.config.eager_eot_threshold)
        else:
            params = {
                "model": self.config.model,
                "language": self.config.language,
                "encoding": self.config.encoding,
                "sample_rate": self.config.sample_rate,
                "channels": 1,
                "interim_results": "true" if self.config.interim_results else "false",
                "punctuate": "true" if self.config.punctuate else "false",
            }

        query_string = urlencode(params)
        return f"{self.config.url}?{query_string}"

    @override
    async def start_connection(self) -> None:
        assert self.config is not None
        self.ten_env.log_info("[DEEPGRAM-WS] Starting WebSocket connection")

        try:
            await self.stop_connection()

            url = self._build_websocket_url()
            self.ten_env.log_info(f"[DEEPGRAM-WS] Connecting to: {url}")

            self.session = aiohttp.ClientSession()
            headers = {"Authorization": f"Token {self.config.api_key}"}

            self.ws = await self.session.ws_connect(
                url,
                headers=headers,
                timeout=aiohttp.ClientTimeout(total=30),
            )

            self.connected = True
            self.ten_env.log_info("[DEEPGRAM-WS] WebSocket connected successfully")
            self.receive_task = asyncio.create_task(self._receive_loop())

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Failed to start connection: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def _receive_loop(self):
        try:
            assert self.ws is not None

            async for msg in self.ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    try:
                        data = json.loads(msg.data)
                        await self._handle_message(data)
                    except json.JSONDecodeError as e:
                        self.ten_env.log_error(f"[DEEPGRAM-WS] JSON decode error: {e}")

                elif msg.type == aiohttp.WSMsgType.ERROR:
                    self.ten_env.log_error(f"[DEEPGRAM-WS] WebSocket error: {msg}")

                elif msg.type == aiohttp.WSMsgType.CLOSED:
                    self.ten_env.log_info("[DEEPGRAM-WS] WebSocket closed by server")
                    break

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error in receive loop: {e}")
        finally:
            self.connected = False

    async def _handle_message(self, data: dict):
        try:
            msg_type = data.get("type")

            if msg_type == "Results":
                await self._handle_transcript(data)

            elif msg_type == "Metadata":
                self.ten_env.log_debug(f"[DEEPGRAM-WS] Metadata: {data}")

            elif msg_type == "UtteranceEnd":
                self.ten_env.log_info("[DEEPGRAM-WS] UtteranceEnd", category=LOG_CATEGORY_VENDOR)
                await self.send_asr_finalize_end()

            elif msg_type == "SpeechStarted":
                self.ten_env.log_info("[DEEPGRAM-WS] SpeechStarted", category=LOG_CATEGORY_VENDOR)

            elif msg_type == "Connected":
                self.ten_env.log_info(
                    f"[DEEPGRAM-FLUX] Connected: {data.get('request_id')}",
                    category=LOG_CATEGORY_VENDOR,
                )

            elif msg_type == "TurnInfo":
                await self._handle_flux_turn_info(data)

            elif msg_type == "EndOfTurn":
                self.ten_env.log_info(
                    f"[DEEPGRAM-FLUX] EndOfTurn event received: {data}",
                    category=LOG_CATEGORY_VENDOR,
                )
                await self.send_asr_finalize_end()

            elif msg_type == "EagerEndOfTurn":
                self.ten_env.log_info(
                    f"[DEEPGRAM-FLUX] EagerEndOfTurn event received: {data}",
                    category=LOG_CATEGORY_VENDOR,
                )

            elif msg_type == "TurnResumed":
                self.ten_env.log_info(
                    f"[DEEPGRAM-FLUX] TurnResumed event received: {data}",
                    category=LOG_CATEGORY_VENDOR,
                )

            else:
                self.ten_env.log_debug(f"[DEEPGRAM-WS] Unknown message type: {msg_type}")

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error handling message: {e}")

    async def _handle_flux_turn_info(self, data: dict):
        try:
            transcript_text = data.get("transcript", "")

            # Skip empty transcripts
            if not transcript_text:
                return

            event_type = data.get("event", "")
            audio_start = data.get("audio_window_start", 0.0)
            audio_end = data.get("audio_window_end", 0.0)

            # Convert to milliseconds
            start_ms = int(audio_start * 1000)
            duration_ms = int((audio_end - audio_start) * 1000)

            # Determine if final (EndOfTurn event or specific event type)
            is_final = event_type == "EndOfTurn"

            # Get confidence from words array
            words = data.get("words", [])
            confidence = words[0].get("confidence", 0.0) if words else 0.0

            # Filter out low-confidence interim results to avoid false positives from noise
            if not is_final and confidence < 0.5:
                return

            # Log transcript
            self.ten_env.log_info(
                f"[DEEPGRAM-FLUX-TRANSCRIPT] Text: '{transcript_text}' | "
                f"Event: {event_type} | Start: {start_ms}ms | Duration: {duration_ms}ms",
                category=LOG_CATEGORY_VENDOR,
            )

            # Send ASR result
            asr_result = ASRResult(
                text=transcript_text,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
            )

            await self.send_asr_result(asr_result)

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-FLUX] Error processing TurnInfo: {e}")

    async def _handle_transcript(self, data: dict):
        try:
            channel = data.get("channel", {})
            alternatives = channel.get("alternatives", [])

            if not alternatives:
                return

            # Get the best alternative
            alternative = alternatives[0]
            transcript_text = alternative.get("transcript", "")

            # Skip empty transcripts
            if not transcript_text:
                return

            # Get timing information
            metadata = data.get("metadata", {})
            is_final = data.get("is_final", False)
            start_time = data.get("start", 0.0)
            duration = data.get("duration", 0.0)

            # Convert to milliseconds
            start_ms = int(start_time * 1000)
            duration_ms = int(duration * 1000)

            # Get confidence for filtering
            confidence = alternative.get("confidence", 0.0)

            # Filter out low-confidence interim results to avoid false positives from noise
            if not is_final and confidence < 0.5:
                return

            # Log transcript
            self.ten_env.log_info(
                f"[DEEPGRAM-TRANSCRIPT] Text: '{transcript_text}' | "
                f"Final: {is_final} | Start: {start_ms}ms | Duration: {duration_ms}ms",
                category=LOG_CATEGORY_VENDOR,
            )

            # Send ASR result
            asr_result = ASRResult(
                text=transcript_text,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
            )

            await self.send_asr_result(asr_result)

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error processing transcript: {e}")

    async def stop_connection(self) -> None:
        try:
            if self.receive_task:
                self.receive_task.cancel()
                try:
                    await self.receive_task
                except asyncio.CancelledError:
                    pass
                self.receive_task = None

            if self.ws and not self.ws.closed:
                await self.ws.close()
                self.ws = None

            if self.session and not self.session.closed:
                await self.session.close()
                self.session = None

            self.connected = False
            self.ten_env.log_info("[DEEPGRAM-WS] Connection stopped")

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error stopping connection: {e}")

    @override
    def is_connected(self) -> bool:
        return self.connected and self.ws is not None and not self.ws.closed

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        assert self.config is not None
        return self.config.sample_rate

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        assert self.config is not None

        if not self.is_connected():
            self.ten_env.log_warn("[DEEPGRAM-WS] Cannot send audio, not connected")
            return False

        assert self.ws is not None

        try:
            buf = frame.lock_buf()
            self.audio_timeline.add_user_audio(
                int(len(buf) / (self.config.sample_rate / 1000 * 2))
            )

            await self.ws.send_bytes(bytes(buf))
            self.audio_frame_count += 1

            frame.unlock_buf(buf)
            return True

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error sending audio: {e}")
            return False
