from datetime import datetime
import json
import os

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
    LOG_CATEGORY_KEY_POINT,
    LOG_CATEGORY_VENDOR,
)

import asyncio
from deepgram import (
    DeepgramClient,
    LiveTranscriptionEvents,
    LiveOptions,
)
import deepgram
from .config import DeepgramASRConfig
from ten_ai_base.dumper import Dumper
from .reconnect_manager import ReconnectManager


class DeepgramASRExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.client: deepgram.AsyncListenWebSocketClient | None = None
        self.config: DeepgramASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.audio_frame_count: int = 0
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0
        self.using_v2: bool = False  # Track if we're using v2 API

        # Reconnection manager with retry limits and backoff strategy
        self.reconnect_manager: ReconnectManager | None = None

    def _is_v2_endpoint(self) -> bool:
        """Detect if we should use v2 API based on URL or model."""
        if not self.config:
            return False
        # Check if URL contains v2 or model is flux
        url_is_v2 = "/v2/" in self.config.url
        model_is_flux = self.config.model.startswith("flux")
        return url_is_v2 or model_is_flux

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    def vendor(self) -> str:
        """Get the name of the ASR vendor."""
        return "deepgram"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = DeepgramASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )
            ten_env.log_info("=" * 60)
            ten_env.log_info("[DEEPGRAM-INIT] Deepgram ASR Configuration")
            ten_env.log_info("=" * 60)
            ten_env.log_info(f"[DEEPGRAM-INIT] Model: {self.config.model}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Language: {self.config.language}")
            ten_env.log_info(f"[DEEPGRAM-INIT] URL: {self.config.url}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Sample Rate: {self.config.sample_rate}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Encoding: {self.config.encoding}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Interim Results: {self.config.interim_results}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Punctuate: {self.config.punctuate}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Finalize Mode: {self.config.finalize_mode}")
            ten_env.log_info(f"[DEEPGRAM-INIT] Params: {self.config.params}")
            ten_env.log_info("=" * 60)

            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = DeepgramASRConfig.model_validate_json("{}")
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
        self.ten_env.log_info("start_connection")

        try:
            await self.stop_connection()

            # Detect if we should use v2 API
            self.using_v2 = self._is_v2_endpoint()

            if self.using_v2:
                self.ten_env.log_info("[DEEPGRAM] Using v2 API for Flux")
                await self._start_connection_v2()
            else:
                self.ten_env.log_info("[DEEPGRAM] Using v1 API")
                await self._start_connection_v1()

        except Exception as e:
            self.ten_env.log_error(
                f"KEYPOINT start_connection failed: invalid vendor config: {e}"
            )
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def _start_connection_v1(self) -> None:
        """Start connection using v1 API (Nova models)."""
        assert self.config is not None

        # Create DeepgramClient for v1 API (SDK 5.x)
        dg_client = DeepgramClient(api_key=self.config.key or self.config.api_key)

        if self.audio_dumper:
            await self.audio_dumper.start()

        keywords = []
        if self.config.hotwords:
            for hw in self.config.hotwords:
                tokens = hw.split("|")
                if len(tokens) == 2 and tokens[1].isdigit():
                    keywords.append(":".join(tokens))  # replase to ":"
                else:
                    self.ten_env.log_warn("invalid hotword format: " + hw)

        # Build connection parameters for SDK 5.x
        connect_params = {
            "model": self.config.model,
            "encoding": self.config.encoding,
            "sample_rate": self.input_audio_sample_rate(),
            "channels": self.input_audio_channels(),
            "language": self.config.language,
            "interim_results": self.config.interim_results,
            "punctuate": self.config.punctuate,
        }

        if keywords:
            connect_params["keywords"] = keywords

        self.ten_env.log_info("=" * 60)
        self.ten_env.log_info("[DEEPGRAM-CONNECT] Connecting to Deepgram v1")
        self.ten_env.log_info("=" * 60)
        self.ten_env.log_info(f"[DEEPGRAM-CONNECT] Model: {self.config.model}")
        self.ten_env.log_info(f"[DEEPGRAM-CONNECT] Parameters: {connect_params}")
        self.ten_env.log_info("=" * 60)

        # Connect using v1 API (SDK 5.x)
        self.client = dg_client.listen.v1.connect(**connect_params)

        # Register v1 event handlers
        await self._register_deepgram_event_handlers()

        # Start listening
        result = await self.client.start()
        if not result:
            self.ten_env.log_error("failed to connect to deepgram v1")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message="failed to connect to deepgram v1",
                )
            )
            asyncio.create_task(self._handle_reconnect())
        else:
            self.ten_env.log_info("=" * 60)
            self.ten_env.log_info(f"[DEEPGRAM-CONNECT] ✅ CONNECTED to Deepgram v1 {self.config.model}")
            self.ten_env.log_info("=" * 60)

    async def _start_connection_v2(self) -> None:
        """Start connection using v2 API (Flux models)."""
        assert self.config is not None

        # Parse Flux parameters from URL query string or advanced_params_json
        flux_params = {}

        # Extract params from URL query string if present
        if "?" in self.config.url:
            from urllib.parse import parse_qs, urlsplit
            url_parts = urlsplit(self.config.url)
            query_params = parse_qs(url_parts.query)
            for key, value in query_params.items():
                flux_params[key] = float(value[0]) if key == "eot_threshold" else int(value[0])

        # Create DeepgramClient for v2 API
        dg_client = DeepgramClient(api_key=self.config.key or self.config.api_key)

        # Build connection parameters
        connect_params = {
            "model": self.config.model,
            "encoding": self.config.encoding,
            "sample_rate": self.input_audio_sample_rate(),
            "channels": self.input_audio_channels(),
            "language": self.config.language,
            "interim_results": self.config.interim_results,
            "punctuate": self.config.punctuate,
        }

        # Add Flux-specific parameters
        connect_params.update(flux_params)

        self.ten_env.log_info("=" * 60)
        self.ten_env.log_info("[DEEPGRAM-CONNECT] Connecting to Deepgram v2 (Flux)")
        self.ten_env.log_info("=" * 60)
        self.ten_env.log_info(f"[DEEPGRAM-CONNECT] Model: {self.config.model}")
        self.ten_env.log_info(f"[DEEPGRAM-CONNECT] Parameters: {connect_params}")
        self.ten_env.log_info("=" * 60)

        # Connect using v2 API
        self.client = dg_client.listen.v2.connect(**connect_params)

        if self.audio_dumper:
            await self.audio_dumper.start()

        # Register v2 event handlers
        await self._register_deepgram_v2_event_handlers()

        # Start listening
        result = await self.client.start()
        if not result:
            self.ten_env.log_error("failed to connect to deepgram v2")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message="failed to connect to deepgram v2",
                )
            )
            asyncio.create_task(self._handle_reconnect())
        else:
            self.ten_env.log_info("=" * 60)
            self.ten_env.log_info(f"[DEEPGRAM-CONNECT] ✅ CONNECTED to Deepgram v2 {self.config.model}")
            self.ten_env.log_info("=" * 60)

    @override
    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_info(
            f"vendor_cmd: finalize start at {self.last_finalize_timestamp}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self._handle_finalize_api()

    async def _register_deepgram_event_handlers(self):
        """Register event handlers for Deepgram WebSocket client (v1)."""
        assert self.client is not None
        self.client.on(
            LiveTranscriptionEvents.Open, self._deepgram_event_handler_on_open
        )
        self.client.on(
            LiveTranscriptionEvents.Close, self._deepgram_event_handler_on_close
        )
        self.client.on(
            LiveTranscriptionEvents.Transcript,
            self._deepgram_event_handler_on_transcript,
        )
        self.client.on(
            LiveTranscriptionEvents.Error, self._deepgram_event_handler_on_error
        )

    async def _register_deepgram_v2_event_handlers(self):
        """Register event handlers for Deepgram v2 WebSocket client (Flux)."""
        assert self.client is not None

        # Register standard events (same as v1)
        self.client.on(
            LiveTranscriptionEvents.Open, self._deepgram_event_handler_on_open
        )
        self.client.on(
            LiveTranscriptionEvents.Close, self._deepgram_event_handler_on_close
        )
        self.client.on(
            LiveTranscriptionEvents.Transcript,
            self._deepgram_event_handler_on_transcript,
        )
        self.client.on(
            LiveTranscriptionEvents.Error, self._deepgram_event_handler_on_error
        )

        # Register Flux-specific events
        try:
            # EndOfTurn event for Flux turn detection
            self.client.on("EndOfTurn", self._deepgram_event_handler_on_end_of_turn)
            self.ten_env.log_info("[DEEPGRAM-V2] Registered EndOfTurn event handler")
        except Exception as e:
            self.ten_env.log_warn(f"Could not register EndOfTurn handler: {e}")

    async def _deepgram_event_handler_on_end_of_turn(self, _, event):
        """Handle the EndOfTurn event from Deepgram Flux."""
        self.ten_env.log_info(
            f"[DEEPGRAM-FLUX] EndOfTurn event received: {event}",
            category=LOG_CATEGORY_VENDOR,
        )
        # Flux detected end of turn - could trigger LLM response here
        # For now, just log it

    async def _handle_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int = 0,
        duration_ms: int = 0,
        language: str = "",
    ):
        """Handle the ASR result from Deepgram ASR."""
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

    async def _deepgram_event_handler_on_open(self, _, event):
        """Handle the open event from Deepgram."""
        self.ten_env.log_info(
            f"vendor_status_changed: on_open event: {event}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        self.connected = True

        # Notify reconnect manager that connection is successful
        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()

    async def _deepgram_event_handler_on_close(self, *args, **kwargs):
        """Handle the close event from Deepgram."""
        self.ten_env.log_info(
            f"vendor_status_changed: on_close, args: {args}, kwargs: {kwargs}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.connected = False

        if not self.stopped:
            self.ten_env.log_warn(
                "Deepgram connection closed unexpectedly. Reconnecting..."
            )
            await self._handle_reconnect()

    async def _deepgram_event_handler_on_transcript(self, _, result):
        """Handle the transcript event from Deepgram."""
        assert self.config is not None

        # SimpleNamespace
        try:
            result_json = result.to_json()
            self.ten_env.log_info(f"[DEEPGRAM-RESPONSE] Transcript event received")
            self.ten_env.log_debug(
                f"vendor_result: on_transcript: {result_json}",
                category=LOG_CATEGORY_VENDOR,
            )
        except AttributeError:
            # SimpleNamespace no have to_json
            self.ten_env.log_error(
                "deepgram event callback on_transcript: SimpleNamespace object (no to_json method)"
            )

        try:
            sentence = result.channel.alternatives[0].transcript

            if not sentence:
                return

            start_ms = int(
                result.start * 1000
            )  # convert seconds to milliseconds
            duration_ms = int(
                result.duration * 1000
            )  # convert seconds to milliseconds
            actual_start_ms = int(
                self.audio_timeline.get_audio_duration_before_time(start_ms)
                + self.sent_user_audio_duration_ms_before_last_reset
            )
            is_final = result.is_final
            language = self.config.language

            self.ten_env.log_info(
                f"[DEEPGRAM-TRANSCRIPT] Text: '{sentence}' | Final: {is_final} | Start: {actual_start_ms}ms | Duration: {duration_ms}ms"
            )
            self.ten_env.log_debug(
                f"deepgram event callback on_transcript: {sentence}, language: {language}, is_final: {is_final}"
            )

            await self._handle_asr_result(
                sentence,
                final=is_final,
                start_ms=actual_start_ms,
                duration_ms=duration_ms,
                language=language,
            )

        except Exception as e:
            self.ten_env.log_error(f"Error processing transcript: {e}")

    async def _deepgram_event_handler_on_error(self, _, error):
        """Handle the error event from Deepgram."""
        self.ten_env.log_error(
            f"vendor_error: {error.to_json()}",
            category=LOG_CATEGORY_VENDOR,
        )

        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=error.to_json(),
            ),
            ModuleErrorVendorInfo(
                vendor=self.vendor(),
                code=str(error.code) if hasattr(error, "code") else "unknown",
                message=(
                    error.message
                    if hasattr(error, "message")
                    else error.to_json()
                ),
            ),
        )

    async def _handle_finalize_api(self):
        """Handle finalize with api mode."""
        assert self.config is not None

        if self.client is None:
            _ = self.ten_env.log_debug("finalize api: client is not connected")
            return

        await self.client.finalize()
        self.ten_env.log_info(
            "vendor_cmd: finalize api completed",
            category=LOG_CATEGORY_VENDOR,
        )

    async def _handle_reconnect(self):
        """
        Handle a single reconnection attempt using the ReconnectManager.
        Connection success is determined by the _deepgram_event_handler_on_open callback.

        This method should be called repeatedly (e.g., after connection closed events)
        until either connection succeeds or max attempts are reached.
        """
        if not self.reconnect_manager:
            self.ten_env.log_error("ReconnectManager not initialized")
            return

        # Check if we can still retry
        if not self.reconnect_manager.can_retry():
            self.ten_env.log_warn("No more reconnection attempts allowed")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message="No more reconnection attempts allowed",
                )
            )
            return

        # Attempt a single reconnection
        success = await self.reconnect_manager.handle_reconnect(
            connection_func=self.start_connection,
            error_handler=self.send_asr_error,
        )

        if success:
            self.ten_env.log_debug(
                "Reconnection attempt initiated successfully"
            )
        else:
            info = self.reconnect_manager.get_attempts_info()
            self.ten_env.log_debug(
                f"Reconnection attempt failed. Status: {info}"
            )

    async def _finalize_end(self) -> None:
        """Handle finalize end logic."""
        if self.last_finalize_timestamp != 0:
            timestamp = int(datetime.now().timestamp() * 1000)
            latency = timestamp - self.last_finalize_timestamp
            self.ten_env.log_debug(
                f"KEYPOINT finalize end at {timestamp}, counter: {latency}"
            )
            self.last_finalize_timestamp = 0
            await self.send_asr_finalize_end()

    async def stop_connection(self) -> None:
        """Stop the Deepgram connection."""
        try:
            if self.client:
                await self.client.finish()
                self.client = None
                self.connected = False
                self.ten_env.log_info("deepgram connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping deepgram connection: {e}")

    @override
    def is_connected(self) -> bool:
        return self.connected and self.client is not None

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
        assert self.client is not None

        buf = frame.lock_buf()
        if self.audio_dumper:
            await self.audio_dumper.push_bytes(bytes(buf))
        self.audio_timeline.add_user_audio(
            int(len(buf) / (self.config.sample_rate / 1000 * 2))
        )
        await self.client.send(bytes(buf))
        self.audio_frame_count += 1
        # Log every 100 frames to avoid spam (~2 seconds of audio at 50fps)
        if self.audio_frame_count % 100 == 0:
            duration_ms = int(len(buf) / (self.config.sample_rate / 1000 * 2))
            total_audio_ms = self.audio_timeline.get_total_user_audio_duration()
            self.ten_env.log_info(f"[DEEPGRAM-AUDIO] Sent frame #{self.audio_frame_count}, {len(buf)} bytes, ~{duration_ms}ms audio, total: {total_audio_ms}ms")
        frame.unlock_buf(buf)

        return True
