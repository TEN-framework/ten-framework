"""Deepgram WebSocket ASR Extension - Direct WebSocket implementation for Nova and Flux."""

import json
import asyncio
import time
import traceback
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
    ModuleErrorCode,
)
from ten_runtime import (
    AsyncTenEnv,
    AudioFrame,
    Cmd,
)
from ten_ai_base.const import (
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
        self._connection_lock: asyncio.Lock = asyncio.Lock()
        self.turn_max_confidence: float = 0.0  # Track max confidence per turn
        self.last_interim_text: str = ""  # Track last interim text
        self.last_interim_confidence: float = (
            0.0  # Track last interim confidence
        )
        self.session_start_time: float = (
            0.0  # Track when ASR session started for echo cancel settling
        )
        # Accumulate is_final=True segments until speech_final=True
        self.accumulated_segments: list[str] = []
        self.current_utterance_start_ms: int = 0
        # Interrupt state - track if we've sent flush for this turn
        self._interrupt_sent: bool = False
        # Silence sender for EOT detection when mic muted
        self.last_audio_frame_time: float = 0.0
        self.silence_sender_task: Optional[asyncio.Task] = None
        self.silence_gap_threshold: float = 0.3  # Start silence after 300ms gap
        self.silence_max_duration: float = 2.0  # Send silence for max 2 seconds

        # Auto-reconnection
        self.reconnect_task: Optional[asyncio.Task] = None
        self.reconnect_delay: float = 1.0  # Initial reconnect delay
        self.max_reconnect_delay: float = 30.0  # Max backoff delay
        self._should_reconnect: bool = True  # Flag to control reconnection

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

            # Log configuration summary
            api_version = (
                "v2 (Flux)" if self.config.is_v2_endpoint() else "v1 (Nova)"
            )
            ten_env.log_info(
                f"[DEEPGRAM-WS] Config: model={self.config.model} lang={self.config.language} "
                f"api={api_version} sample_rate={self.config.sample_rate}"
            )
            if self.config.is_v2_endpoint():
                ten_env.log_info(
                    f"[DEEPGRAM-WS] v2 params: eot_threshold={self.config.eot_threshold} "
                    f"eot_timeout={self.config.eot_timeout_ms}ms"
                )

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
        if self.config is None:
            raise RuntimeError("Configuration not initialized")

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
                params["eager_eot_threshold"] = str(
                    self.config.eager_eot_threshold
                )
        else:
            params = {
                "model": self.config.model,
                "language": self.config.language,
                "encoding": self.config.encoding,
                "sample_rate": self.config.sample_rate,
                "channels": 1,
                "interim_results": (
                    "true" if self.config.interim_results else "false"
                ),
                "punctuate": "true" if self.config.punctuate else "false",
                "endpointing": self.config.endpointing,  # Pass as integer, not string
                "utterance_end_ms": self.config.utterance_end_ms,  # Pass as integer
            }

        query_string = urlencode(params)
        return f"{self.config.url}?{query_string}"

    @override
    async def start_connection(self) -> None:
        if self.config is None:
            raise RuntimeError("Configuration not initialized")
        self.ten_env.log_info("[DEEPGRAM-WS] Starting WebSocket connection")

        async with self._connection_lock:
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
                self.session_start_time = (
                    time.time()
                )  # Track session start for echo cancel settling
                self.ten_env.log_info(
                    "[DEEPGRAM-WS] WebSocket connected successfully"
                )
                self.receive_task = asyncio.create_task(self._receive_loop())

                # Start silence sender for EOT detection when mic muted
                if (
                    not self.silence_sender_task
                    or self.silence_sender_task.done()
                ):
                    self.silence_sender_task = asyncio.create_task(
                        self._silence_sender()
                    )

                # Start reconnect monitor (only once)
                if not self.reconnect_task or self.reconnect_task.done():
                    self._should_reconnect = True
                    self.reconnect_task = asyncio.create_task(
                        self._reconnect_monitor()
                    )

            except Exception as e:
                self.ten_env.log_error(
                    f"[DEEPGRAM-WS] Failed to start connection: {e}\n{traceback.format_exc()}"
                )
                await self.send_asr_error(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=str(e),
                    ),
                )

    async def _receive_loop(self):
        try:
            if self.ws is None:
                raise RuntimeError("WebSocket not initialized")

            async for msg in self.ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    try:
                        data = json.loads(msg.data)
                        await self._handle_message(data)
                    except json.JSONDecodeError as e:
                        self.ten_env.log_error(
                            f"[DEEPGRAM-WS] JSON decode error: {e}"
                        )

                elif msg.type == aiohttp.WSMsgType.ERROR:
                    self.ten_env.log_error(
                        f"[DEEPGRAM-WS] WebSocket error: {msg}"
                    )

                elif msg.type == aiohttp.WSMsgType.CLOSED:
                    self.ten_env.log_info(
                        "[DEEPGRAM-WS] WebSocket closed by server"
                    )
                    break

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error in receive loop: {e}")
        finally:
            self.connected = False

    async def _silence_sender(self):
        """
        Send silence when real audio stops, to trigger Deepgram's natural EOT detection.

        When the user mutes their mic (e.g., TV noise causing endless interim transcripts),
        Deepgram never sees silence to trigger speech_final=True. This task monitors for
        audio gaps and sends silence frames to let Deepgram naturally detect end-of-turn.

        Also sends periodic keep-alive silence to prevent WebSocket timeout when idle.
        """
        # 10ms of silence at 16kHz mono (16-bit = 2 bytes per sample)
        # 16000 samples/sec * 0.01 sec * 2 bytes = 320 bytes
        silence_frame = bytes(320)
        silence_start_time = 0.0
        last_keepalive_time = 0.0
        keepalive_interval = 5.0  # Send keep-alive every 5 seconds when idle

        while True:
            await asyncio.sleep(0.01)  # 10ms intervals

            # Check if we should send silence
            if not self.is_connected() or self.ws is None:
                continue

            now = time.time()

            # If no real audio for 300ms and we have pending speech
            if (
                self.last_audio_frame_time > 0
                and (now - self.last_audio_frame_time)
                > self.silence_gap_threshold
                and (self.last_interim_text or self.accumulated_segments)
            ):
                # Start tracking silence duration
                if silence_start_time == 0.0:
                    silence_start_time = now
                    self.ten_env.log_info(
                        f"[DEEPGRAM-SILENCE] Starting silence sender: "
                        f"gap={now - self.last_audio_frame_time:.1f}s, "
                        f"interim='{self.last_interim_text}', "
                        f"accumulated={len(self.accumulated_segments)} segments"
                    )

                # Stop after max duration
                silence_elapsed = now - silence_start_time
                if silence_elapsed > self.silence_max_duration:
                    self.ten_env.log_info(
                        f"[DEEPGRAM-SILENCE] Max duration reached ({self.silence_max_duration}s), "
                        f"stopping silence sender"
                    )
                    silence_start_time = 0.0
                    continue

                # Send silence frame
                try:
                    await self.ws.send_bytes(silence_frame)
                    last_keepalive_time = now
                except Exception as e:
                    self.ten_env.log_error(
                        f"[DEEPGRAM-SILENCE] Error sending silence: {e}"
                    )

            else:
                # Reset silence tracking when real audio resumes
                if silence_start_time > 0.0:
                    self.ten_env.log_info(
                        "[DEEPGRAM-SILENCE] Real audio resumed, stopping silence sender"
                    )
                    silence_start_time = 0.0

                # Send periodic keep-alive when no audio for extended period
                # This prevents WebSocket timeout when user is muted for long time
                if (
                    self.last_audio_frame_time > 0
                    and (now - self.last_audio_frame_time) > keepalive_interval
                    and (now - last_keepalive_time) > keepalive_interval
                ):
                    try:
                        await self.ws.send_bytes(silence_frame)
                        last_keepalive_time = now
                        self.ten_env.log_debug(
                            "[DEEPGRAM-SILENCE] Sent keep-alive silence"
                        )
                    except Exception as e:
                        self.ten_env.log_error(
                            f"[DEEPGRAM-SILENCE] Error sending keep-alive: {e}"
                        )

    async def _reconnect_monitor(self):
        """Monitor connection and auto-reconnect if dropped."""
        current_delay = self.reconnect_delay

        while self._should_reconnect:
            await asyncio.sleep(2.0)  # Check every 2 seconds

            if not self._should_reconnect:
                break

            if not self.is_connected():
                self.ten_env.log_warn(
                    f"[DEEPGRAM-RECONNECT] Connection lost, attempting reconnect "
                    f"in {current_delay:.1f}s"
                )

                await asyncio.sleep(current_delay)

                if not self._should_reconnect:
                    break

                try:
                    await self.start_connection()
                    if self.is_connected():
                        self.ten_env.log_info(
                            "[DEEPGRAM-RECONNECT] Successfully reconnected"
                        )
                        current_delay = (
                            self.reconnect_delay
                        )  # Reset delay on success
                    else:
                        # Exponential backoff
                        current_delay = min(
                            current_delay * 2, self.max_reconnect_delay
                        )
                except Exception as e:
                    self.ten_env.log_error(
                        f"[DEEPGRAM-RECONNECT] Reconnection failed: {e}"
                    )
                    current_delay = min(
                        current_delay * 2, self.max_reconnect_delay
                    )

    async def _handle_message(self, data: dict):
        try:
            msg_type = data.get("type")

            if msg_type == "Results":
                await self._handle_transcript(data)

            elif msg_type == "Metadata":
                self.ten_env.log_debug(f"[DEEPGRAM-WS] Metadata: {data}")

            elif msg_type == "UtteranceEnd":
                self.ten_env.log_info(
                    "[DEEPGRAM-WS] UtteranceEnd", category=LOG_CATEGORY_VENDOR
                )
                # Flush any accumulated segments as final before clearing
                if self.accumulated_segments:
                    combined_text = " ".join(self.accumulated_segments)
                    self.ten_env.log_info(
                        f"[DEEPGRAM-UTTERANCE-END-FLUSH] Flushing {len(self.accumulated_segments)} accumulated segments: '{combined_text}'",
                        category=LOG_CATEGORY_VENDOR,
                    )

                    asr_result = ASRResult(
                        text=combined_text,
                        final=True,
                        start_ms=self.current_utterance_start_ms,
                        duration_ms=0,  # Unknown duration for flushed segments
                        language=self.config.language,
                        words=[],
                    )

                    self.ten_env.log_info(
                        f"[DEEPGRAM-SENDING-ASR] text={asr_result.text!r} final={asr_result.final}"
                    )
                    await self.send_asr_result(asr_result)

                # Clear accumulated segments on utterance end
                self.accumulated_segments = []
                self.current_utterance_start_ms = 0
                await self.send_asr_finalize_end()

            elif msg_type == "SpeechStarted":
                self.ten_env.log_info(
                    "[DEEPGRAM-WS] SpeechStarted", category=LOG_CATEGORY_VENDOR
                )
                # Clear accumulated segments on new speech
                self.accumulated_segments = []
                self.current_utterance_start_ms = 0

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

            elif msg_type in ["Error", "Warning", "error", "warning"]:
                # Log errors/warnings at INFO level with full payload
                self.ten_env.log_info(f"[DEEPGRAM-{msg_type.upper()}] {data}")

            else:
                # Log unknown messages with full payload to catch any issues
                self.ten_env.log_info(
                    f"[DEEPGRAM-WS] Unknown message type '{msg_type}': {data}"
                )

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

            # Reset tracking on StartOfTurn
            if event_type == "StartOfTurn":
                self.turn_max_confidence = confidence
                self.last_interim_text = transcript_text
                self.last_interim_confidence = confidence

            # Track interim results
            if not is_final:
                self.turn_max_confidence = max(
                    self.turn_max_confidence, confidence
                )
                # Update last interim if it passed the confidence filter
                if confidence >= self.config.min_interim_confidence:
                    self.last_interim_text = transcript_text
                    self.last_interim_confidence = confidence

            # Calculate word count for filtering
            word_count = len(transcript_text.split())

            # Filter single-word transcripts (interims AND finals) during echo cancel settling
            if self.session_start_time > 0:
                elapsed_time = time.time() - self.session_start_time

                if (
                    word_count == 1
                    and elapsed_time < self.config.echo_cancel_duration
                ):
                    self.ten_env.log_warn(
                        f"[DEEPGRAM-FLUX-FILTER] Dropping single-word {'final' if is_final else 'interim'} during echo cancel settling: "
                        f"text='{transcript_text}', confidence={confidence:.3f}, "
                        f"elapsed={elapsed_time:.1f}s, word_count={word_count}"
                    )
                    if is_final:
                        self.turn_max_confidence = 0.0
                        self.last_interim_text = ""
                        self.last_interim_confidence = 0.0
                    return

            # Apply confidence filtering ONLY to single-word results (multi-word sentences always pass)
            # Single words are more likely to be false positives from echo/noise
            if word_count == 1:
                # Filter out low-confidence interim results to avoid false positives from noise
                if (
                    not is_final
                    and confidence < self.config.min_interim_confidence
                ):
                    self.ten_env.log_warn(
                        f"[DEEPGRAM-FLUX-FILTER] Dropping low-confidence single-word interim: "
                        f"text='{transcript_text}', confidence={confidence:.3f}, "
                        f"threshold={self.config.min_interim_confidence:.3f}"
                    )
                    return

                # Filter out finals with low confidence (catches "Hey," -> "I" with 0.165 confidence)
                if is_final and confidence < self.config.min_interim_confidence:
                    self.ten_env.log_warn(
                        f"[DEEPGRAM-FLUX-FILTER] Dropping low-confidence single-word final: "
                        f"text='{transcript_text}', final_confidence={confidence:.3f}, "
                        f"last_interim='{self.last_interim_text}' (conf={self.last_interim_confidence:.3f}), "
                        f"threshold={self.config.min_interim_confidence:.3f}"
                    )
                    self.turn_max_confidence = 0.0
                    self.last_interim_text = ""
                    self.last_interim_confidence = 0.0
                    return

            # Log transcript with confidence for debugging false positives
            self.ten_env.log_info(
                f"[DEEPGRAM-FLUX-TRANSCRIPT] Text: '{transcript_text}' | "
                f"Event: {event_type} | Start: {start_ms}ms | Duration: {duration_ms}ms | "
                f"Confidence: {confidence:.3f}",
                category=LOG_CATEGORY_VENDOR,
            )

            # Send ASR result
            asr_result = ASRResult(
                text=transcript_text,
                final=is_final,
                start_ms=start_ms,
                duration_ms=duration_ms,
                language=self.config.language,
                words=[],
            )

            await self.send_asr_result(asr_result)

            # Reset tracking after sending final
            if is_final:
                self.turn_max_confidence = 0.0
                self.last_interim_text = ""
                self.last_interim_confidence = 0.0

        except Exception as e:
            self.ten_env.log_error(
                f"[DEEPGRAM-FLUX] Error processing TurnInfo: {e}"
            )

    async def _handle_transcript(self, data: dict):
        try:
            channel = data.get("channel", {})
            alternatives = channel.get("alternatives", [])

            if not alternatives:
                return

            # Get the best alternative
            alternative = alternatives[0]
            transcript_text = alternative.get("transcript", "")
            # Log ALL transcripts including empties
            _is_final = data.get("is_final", False)
            _speech_final = data.get(
                "speech_final", None
            )  # Check for speech_final field
            _confidence = alternative.get("confidence", 0.0)

            # Log Deepgram response
            self.ten_env.log_info(
                f"[DEEPGRAM-TRANSCRIPT] text={transcript_text!r} is_final={_is_final} "
                f"speech_final={_speech_final} conf={_confidence:.3f}"
            )

            # Skip empty transcripts
            if not transcript_text:
                return

            # Get timing information
            is_final = data.get("is_final", False)
            speech_final = data.get("speech_final", False)
            start_time = data.get("start", 0.0)
            duration = data.get("duration", 0.0)

            # Convert to milliseconds
            start_ms = int(start_time * 1000)
            duration_ms = int(duration * 1000)

            # Get confidence for filtering
            confidence = alternative.get("confidence", 0.0)

            # Calculate word count for interrupt check
            word_count = len(transcript_text.split())

            # CRITICAL FIX: Accumulate segments until speech_final=True
            # According to Deepgram docs on endpointing + interim_results:
            # - Interim results (is_final=False): Send immediately for streaming UX
            # - is_final=True, speech_final=False: Transcript finalized but speech continues
            # - is_final=True, speech_final=True: Speech ended after endpointing delay (500ms)
            # We accumulate all is_final segments and only send to LLM when speech_final=True

            # Track utterance start for combined segment
            if not self.accumulated_segments and not is_final:
                self.current_utterance_start_ms = start_ms

            # Handle different states
            if not is_final:
                # Interim result - send immediately for streaming UX
                if confidence < self.config.min_interim_confidence:
                    return  # Filter low-confidence interim

                # Send interrupt flush on first confident interim with enough words
                if (
                    self.config.interrupt_on_speech
                    and not self._interrupt_sent
                    and confidence >= self.config.interrupt_min_confidence
                    and word_count >= self.config.interrupt_min_words
                ):
                    self._interrupt_sent = True
                    await self.ten_env.send_cmd(Cmd.create("flush"))
                    self.ten_env.log_debug(
                        f"[DEEPGRAM-INTERRUPT] Sent flush: "
                        f"'{transcript_text}' conf={confidence:.3f}"
                    )

                # PREPEND accumulated segments for smooth UX
                # User sees: "One, two, three, four" instead of just "four" when new segment starts
                if self.accumulated_segments:
                    display_text = " ".join(
                        self.accumulated_segments + [transcript_text]
                    )
                    self.ten_env.log_info(
                        f"[DEEPGRAM-INTERIM-COMBINED] Accumulated: {len(self.accumulated_segments)} segments | "
                        f"Current: '{transcript_text}' | Display: '{display_text}'",
                        category=LOG_CATEGORY_VENDOR,
                    )
                else:
                    display_text = transcript_text
                    self.ten_env.log_info(
                        f"[DEEPGRAM-INTERIM] Text: '{transcript_text}' | "
                        f"conf: {confidence:.3f} | Start: {start_ms}ms | Duration: {duration_ms}ms",
                        category=LOG_CATEGORY_VENDOR,
                    )

                asr_result = ASRResult(
                    text=display_text,
                    final=False,
                    start_ms=(
                        self.current_utterance_start_ms
                        if self.accumulated_segments
                        else start_ms
                    ),
                    duration_ms=duration_ms,
                    language=self.config.language,
                    words=[],
                )
                await self.send_asr_result(asr_result)

            elif is_final and not speech_final:
                # Segment finalized but speech continues - accumulate it
                if not self.accumulated_segments:
                    self.current_utterance_start_ms = start_ms

                self.accumulated_segments.append(transcript_text)
                self.ten_env.log_info(
                    f"[DEEPGRAM-ACCUMULATE] Segment {len(self.accumulated_segments)}: '{transcript_text}' | "
                    f"Total accumulated: {len(self.accumulated_segments)} segments",
                    category=LOG_CATEGORY_VENDOR,
                )

            elif speech_final:
                # Speech ended - combine all accumulated + current and send as final
                if self.accumulated_segments:
                    # Combine accumulated segments + current
                    combined_text = " ".join(
                        self.accumulated_segments + [transcript_text]
                    )
                    self.ten_env.log_info(
                        f"[DEEPGRAM-COMBINE] Combined {len(self.accumulated_segments)+1} segments: '{combined_text}'",
                        category=LOG_CATEGORY_VENDOR,
                    )
                else:
                    # No accumulated segments, just current
                    combined_text = transcript_text
                    self.current_utterance_start_ms = start_ms

                # Calculate total duration from utterance start to now
                total_duration_ms = (
                    start_ms + duration_ms
                ) - self.current_utterance_start_ms

                self.ten_env.log_info(
                    f"[DEEPGRAM-FINAL] Text: '{combined_text}' | "
                    f"speech_final: True | Start: {self.current_utterance_start_ms}ms | Duration: {total_duration_ms}ms",
                    category=LOG_CATEGORY_VENDOR,
                )

                asr_result = ASRResult(
                    text=combined_text,
                    final=True,
                    start_ms=self.current_utterance_start_ms,
                    duration_ms=total_duration_ms,
                    language=self.config.language,
                    words=[],
                )

                self.ten_env.log_info(
                    f"[DEEPGRAM-SENDING-ASR] text={asr_result.text!r} final={asr_result.final}"
                )
                await self.send_asr_result(asr_result)

                # Clear accumulated segments for next utterance
                self.accumulated_segments = []
                self.current_utterance_start_ms = 0
                # Reset interrupt state for next turn
                self._interrupt_sent = False

        except Exception as e:
            self.ten_env.log_error(
                f"[DEEPGRAM-WS] Error processing transcript: {e}"
            )

    async def stop_connection(self) -> None:
        try:
            # Stop reconnect monitor first
            self._should_reconnect = False
            if self.reconnect_task:
                self.reconnect_task.cancel()
                try:
                    await self.reconnect_task
                except asyncio.CancelledError:
                    pass
                self.reconnect_task = None

            # Stop silence sender task
            if self.silence_sender_task:
                self.silence_sender_task.cancel()
                try:
                    await self.silence_sender_task
                except asyncio.CancelledError:
                    pass
                self.silence_sender_task = None

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
                try:
                    await asyncio.wait_for(self.session.close(), timeout=5.0)
                except asyncio.TimeoutError:
                    if self.ten_env:
                        self.ten_env.log_warn(
                            "[DEEPGRAM-WS] Session close timed out"
                        )
                self.session = None

            self.connected = False
            if self.ten_env:
                self.ten_env.log_info("[DEEPGRAM-WS] Connection stopped")

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(
                    f"[DEEPGRAM-WS] Error stopping connection: {e}\n{traceback.format_exc()}"
                )

    @override
    def is_connected(self) -> bool:
        return self.connected and self.ws is not None and not self.ws.closed

    @override
    def buffer_strategy(self) -> ASRBufferConfig:
        return ASRBufferConfigModeKeep(byte_limit=1024 * 1024 * 10)

    @override
    def input_audio_sample_rate(self) -> int:
        if self.config is None:
            raise RuntimeError("Configuration not initialized")
        return self.config.sample_rate

    @override
    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        if self.config is None:
            raise RuntimeError("Configuration not initialized")

        if not self.is_connected():
            self.ten_env.log_warn(
                "[DEEPGRAM-WS] Cannot send audio, not connected"
            )
            return False

        if self.ws is None:
            raise RuntimeError("WebSocket not initialized")

        try:
            buf = frame.lock_buf()
            self.audio_timeline.add_user_audio(
                int(len(buf) / (self.config.sample_rate / 1000 * 2))
            )

            await self.ws.send_bytes(bytes(buf))
            self.audio_frame_count += 1

            # Track last audio frame time for silence sender
            self.last_audio_frame_time = time.time()

            frame.unlock_buf(buf)
            return True

        except Exception as e:
            self.ten_env.log_error(f"[DEEPGRAM-WS] Error sending audio: {e}")
            return False

    @override
    async def finalize(self, session_id: str | None) -> None:
        """Finalize recognition session and flush remaining audio."""
        if self.ws and not self.ws.closed:
            try:
                # Send FinalizeSpeech message to indicate end of audio
                await self.ws.send_text('{"type":"FinalizeSpeech"}')
                self.ten_env.log_info("[DEEPGRAM-WS] Finalization sent")
            except Exception as e:
                self.ten_env.log_error(
                    f"[DEEPGRAM-WS] Error during finalize: {e}"
                )
