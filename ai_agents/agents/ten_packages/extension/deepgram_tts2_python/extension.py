#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import traceback
import os
import time

from ten_ai_base.struct import TTSTextInput
from ten_ai_base.message import TTSAudioEndReason
from ten_ai_base.tts2 import AsyncTTS2BaseExtension
from .deepgram_tts import DeepgramTTS, DeepgramTTSConfig
from ten_runtime import (
    AsyncTenEnv,
)


class DeepgramTTSExtension(AsyncTTS2BaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config = None
        self.client = None
        self.websocket_ready = False  # Track if WebSocket is ready
        self.pending_requests = []  # Queue for requests received before WebSocket is ready

        # Flush handling state
        self.flush_requested = False
        
        # Request completion tracking
        self.completed_request_ids = set()
        self.pending_flush_data = None
        self.pending_flush_ten_env = None   # Track if flush was requested

        # Circuit breaker for connection resilience
        self.circuit_breaker = {
            'failure_count': 0,
            'last_failure_time': 0,
            'state': 'CLOSED',  # CLOSED, OPEN, HALF_OPEN
            'failure_threshold': 5,
            'recovery_timeout': 30  # seconds
        }

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        """Initialize the extension - moved from on_start as per PR feedback"""
        try:
            await super().on_init(ten_env)
            ten_env.log_debug("DeepgramTTS on_init - INITIALIZING")

            # Use TEN framework method to read config as per PR feedback
            config_json, _ = await ten_env.get_property_to_json("")
            self.config = await DeepgramTTSConfig.create_async(ten_env=ten_env)
            ten_env.log_info(f"DEBUG: Received config - api_key: [{self.config.api_key}], type: {type(self.config.api_key)}")



            if not self.config.api_key:
                # Send fatal error using TTS2 base class method
                await self._send_initialization_error("Deepgram API key is required")
                return

            ten_env.log_info(f"Initializing Deepgram TTS with model: {self.config.model}, voice: {self.config.voice}")
            self.client = DeepgramTTS(self.config)

            # Initialize persistent WebSocket connection
            await self.client.initialize(ten_env)

            # Mark WebSocket as ready
            self.websocket_ready = True
            ten_env.log_info("KEYPOINT: WebSocket connection established - ready to process TTS requests")

            ten_env.log_info("DeepgramTTS extension initialized successfully with WebSocket")

        except Exception as e:
            ten_env.log_error(f"Failed to initialize Deepgram TTS: {str(e)}")
            ten_env.log_error(f"Traceback: {traceback.format_exc()}")
            # Send fatal error for any other initialization failures
            await self._send_initialization_error(f"Failed to initialize Deepgram TTS: {str(e)}")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Start the extension - only call parent on_start"""
        try:
            ten_env.log_info("DeepgramTTS extension on_start called")
            await super().on_start(ten_env)
            ten_env.log_info("DeepgramTTS extension started successfully")

        except Exception as e:
            ten_env.log_error(f"Failed to start Deepgram TTS: {str(e)}")
            ten_env.log_error(f"Traceback: {traceback.format_exc()}")

    async def _send_initialization_error(self, message: str):
        """Send initialization error using TTS2 base class method"""
        try:
            # Fixed ModuleError structure as per PR feedback
            from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo, ModuleType

            error_info = ModuleErrorVendorInfo(
                vendor="deepgram",
                code="INITIALIZATION_ERROR",
                message=message
            )

            error = ModuleError(
                message=message,
                module_name=ModuleType.TTS,
                code=ModuleErrorCode.FATAL_ERROR,
                vendor_info=error_info
            )

            # Use TTS2 base class error sending method
            await self.send_tts_error(None, error)  # No request_id for initialization errors
            self.ten_env.log_error(f"Sent initialization error: {message}")

        except Exception as e:
            self.ten_env.log_error(f"Failed to send initialization error: {str(e)}")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_debug("DeepgramTTS on_stop")

        # Cleanup client connection
        if self.client:
            await self.client.cleanup()

    def vendor(self) -> str:
        return "deepgram"

    async def request_tts(self, t: TTSTextInput) -> None:
        """Handle TTS request using TTS2 interface"""
        try:
            self.ten_env.log_info(f"Received TTS request: {t.request_id} - {t.text[:50]}...")
            
            # Check if a request has already been completed
            if t.request_id in self.completed_request_ids:
                self.ten_env.log_error(f"Rejecting request {t.request_id} - session already completed")
                from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
                
                error_info = ModuleErrorVendorInfo(
                    vendor="deepgram",
                    code="duplicate_request_id",
                    message="Request ID has already been processed"
                )
                
                error = ModuleError(
                    message="Cannot process duplicate request_id",
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    vendor_info=error_info
                )
                
                await self.send_tts_error(t.request_id, error)
                return

            # Text validation - check for invalid text that cannot be synthesized
            if not self._is_valid_text(t.text):
                self.ten_env.log_error(f"Invalid text received: '{t.text}' - returning NON_FATAL_ERROR")
                from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo

                error_info = ModuleErrorVendorInfo(
                    vendor="deepgram",
                    code="invalid_text",
                    message=f"Text cannot be synthesized: '{t.text}'"
                )

                error = ModuleError(
                    message="Invalid text for TTS synthesis",
                    module="tts",
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    vendor_info=error_info
                )

                await self.send_tts_error(t.request_id, error)
                return

            # Check circuit breaker before processing
            if not self._should_allow_request():
                self.ten_env.log_error("KEYPOINT: Circuit breaker OPEN - rejecting TTS request")
                from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo, ModuleType

                error_info = ModuleErrorVendorInfo(
                    vendor="deepgram",
                    code="SERVICE_UNAVAILABLE",
                    message="Service temporarily unavailable due to connection issues"
                )

                error = ModuleError(
                    message="Service temporarily unavailable due to connection issues",
                    module_name=ModuleType.TTS,
                    code=ModuleErrorCode.NON_FATAL_ERROR,
                    vendor_info=error_info
                )
                await self.send_tts_error(t.request_id, error)
                return

            # If WebSocket is not ready yet, queue the request
            if not self.websocket_ready:
                self.ten_env.log_info("WebSocket not ready yet, queuing TTS request for later processing")
                self.pending_requests.append(t)
                return

            # Process the request immediately if WebSocket is ready
            await self._process_tts_request(t)

        except Exception as e:
            self.ten_env.log_error(f"request_tts failed: {traceback.format_exc()}")
            self._record_failure()  # Record failure for circuit breaker

            # Send error notification with fixed ModuleError structure
            from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo, ModuleType

            error_info = ModuleErrorVendorInfo(
                vendor="deepgram",
                code="REQUEST_PROCESSING_ERROR",
                message=f"Failed to process TTS request: {str(e)}"
            )

            error = ModuleError(
                message=f"Failed to process TTS request: {str(e)}",
                module_name=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=error_info
            )
            await self.send_tts_error(t.request_id, error)

    async def _process_tts_request(self, t: TTSTextInput) -> None:
        """Process TTS request using persistent WebSocket connection"""
        self.ten_env.log_info(f"DEBUG: Starting TTS request processing for request_id: {t.request_id}, text: {t.text[:50]}...")
        try:
            if not self.client:
                self.ten_env.log_error("Deepgram client not initialized")
                return

            self.ten_env.log_info(f"Processing TTS request: {t.request_id} - {t.text[:50]}...")

            self.ten_env.log_info(f"KEYPOINT: TTS request processing started - {t.request_id}")

            # Record start time for TTFB metrics
            start_time = time.time()
            first_chunk_received = False

            # Send TTS audio start event
            await self.send_tts_audio_start(t.request_id)

            # Send TTS text result
            from ten_ai_base.struct import TTSTextResult
            text_result = TTSTextResult(
                request_id=t.request_id,
                text=t.text,
                start_ms=0,
                duration_ms=0,  # Will be updated when we know the total duration
                words=None,  # Deepgram doesn't provide word-level timing in this mode
                text_result_end=True,
                metadata={}
            )
            await self.send_tts_text_result(text_result)

            # Stream audio data from Deepgram using persistent connection
            total_audio_duration_ms = 0
            chunk_count = 0

            async for audio_chunk in self.client.get(self.ten_env, t.text):
                chunk_count += 1

                # Send TTFB metrics for first chunk
                if not first_chunk_received:
                    ttfb_ms = int((time.time() - start_time) * 1000)
                    await self.send_tts_ttfb_metrics(t.request_id, ttfb_ms)
                    first_chunk_received = True

                    # KEYPOINT logging as per TTS standard
                    # Format: TTS [ttfb:100ms] [text:你好你好] [audio_chunk_bytes:1024] [audio_chunk_duration:200ms] [voice_type:xxx]
                    chunk_duration_ms = len(audio_chunk) / (self.config.sample_rate * 2) * 1000
                    self.ten_env.log_info(f"KEYPOINT: TTS [ttfb:{ttfb_ms}ms] [text:{t.text[:20]}...] [audio_chunk_bytes:{len(audio_chunk)}] [audio_chunk_duration:{chunk_duration_ms:.0f}ms] [voice_type:{self.config.voice}]")

                # Send audio data using TTS2 interface
                await self.send_tts_audio_data(audio_chunk)

                # Dump audio data if enabled
                await self._dump_audio_if_enabled(audio_chunk, t.request_id)

                # Estimate audio duration (rough calculation)
                # For 24kHz, 16-bit, mono: bytes / (24000 * 2) * 1000 = ms
                chunk_duration_ms = len(audio_chunk) / (self.config.sample_rate * 2) * 1000
                total_audio_duration_ms += chunk_duration_ms

            # Send TTS audio end event with correct reason
            request_duration_ms = int((time.time() - start_time) * 1000)

            print(f"DEBUG: flush_requested = {self.flush_requested}")
            # Fixed: moved debug after reason assignment
            # Reason 2 = flush requested, Reason 1 = normal completion
            reason = TTSAudioEndReason.INTERRUPTED if self.flush_requested else TTSAudioEndReason.REQUEST_END
            print(f"DEBUG: reason enum = {reason}, reason.value = {reason.value}")

            print(f"DEBUG: About to call send_tts_audio_end with reason={reason}, reason.value={reason.value}")
            await self.send_tts_audio_end(
                t.request_id,
                request_duration_ms,
                int(total_audio_duration_ms),
                -1,
                reason
            )
            
            # Mark request as completed
            self.completed_request_ids.add(t.request_id)
            self.ten_env.log_info(f"Request {t.request_id} added to completed set")

            # If flush was requested, now call base class to send tts_flush_end
            if self.flush_requested and self.pending_flush_data:
                self.ten_env.log_info("KEYPOINT: Sending tts_flush_end after tts_audio_end")
                await super().on_data(self.pending_flush_ten_env, self.pending_flush_data)
                self.pending_flush_data = None
                self.pending_flush_ten_env = None

            # Note: Do NOT send drain command here - it should only be sent in response to flush command
            # The TTS protocol expects tts_audio_end first, then tts_flush_end only after receiving flush command

            # Record successful operation for circuit breaker
            self._record_success()

            self.ten_env.log_info(f"TTS request completed: {t.request_id} - {chunk_count} chunks, {total_audio_duration_ms:.0f}ms audio")

        except Exception as e:
            self.ten_env.log_error(f"TTS request processing failed: {str(e)}")
            self.ten_env.log_error(f"Traceback: {traceback.format_exc()}")

            # Record failure for circuit breaker
            self._record_failure()

            # Send error notification with fixed ModuleError structure
            from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo, ModuleType

            error_info = ModuleErrorVendorInfo(
                vendor="deepgram",
                code="PROCESSING_ERROR",
                message=f"Failed to process TTS request: {str(e)}"
            )

            error = ModuleError(
                message=f"Failed to process TTS request: {str(e)}",
                module_name=ModuleType.TTS,
                code=ModuleErrorCode.NON_FATAL_ERROR,
                vendor_info=error_info
            )
            await self.send_tts_error(t.request_id, error)

    async def _dump_audio_if_enabled(self, audio_data: bytes, request_id: str = "default"):
        """Dump audio data if dump is enabled"""
        try:
            # Check if dump is enabled in config
            self.ten_env.log_info(f"DEBUG: Checking dump config - hasattr(dump_enabled): {hasattr(self.config, 'dump_enabled')}, dump_enabled: {getattr(self.config, 'dump_enabled', 'NOT_SET')}")
            if hasattr(self.config, 'dump_enabled') and self.config.dump_enabled:
                # Fixed dump path default as per PR feedback
                dump_path = getattr(self.config, 'dump_path', '') + f"/deepgram_tts2_python_out_{request_id}.pcm"

                # Ensure directory exists
                import os
                os.makedirs(os.path.dirname(dump_path), exist_ok=True)

                # Append audio data to dump file
                with open(dump_path, 'ab') as f:
                    f.write(audio_data)

                self.ten_env.log_debug(f"Dumped {len(audio_data)} bytes to {dump_path}")
        except Exception as e:
            self.ten_env.log_error(f"Failed to dump audio data: {str(e)}")

    def _should_allow_request(self) -> bool:
        """
        Circuit breaker pattern - prevents requests during system failures.

        This implements a basic circuit breaker to protect against cascading failures
        when the Deepgram API is experiencing issues. The circuit breaker has three states:
        - CLOSED: Normal operation, requests are allowed
        - OPEN: Too many failures detected, requests are rejected
        - HALF_OPEN: Testing if service has recovered

        The circuit breaker tracks failure counts and automatically transitions between
        states based on failure thresholds and recovery timeouts.

        Returns:
            bool: True if request should be allowed, False if circuit breaker is open
        """
        import time

        current_time = time.time()

        if self.circuit_breaker['state'] == 'CLOSED':
            return True
        elif self.circuit_breaker['state'] == 'OPEN':
            # Check if recovery timeout has passed
            if current_time - self.circuit_breaker['last_failure_time'] > self.circuit_breaker['recovery_timeout']:
                self.circuit_breaker['state'] = 'HALF_OPEN'
                self.ten_env.log_info("KEYPOINT: Circuit breaker transitioning to HALF_OPEN state")
                return True
            return False
        elif self.circuit_breaker['state'] == 'HALF_OPEN':
            return True

        return False

    def _record_success(self):
        """Record successful operation"""
        if self.circuit_breaker['state'] == 'HALF_OPEN':
            self.circuit_breaker['state'] = 'CLOSED'
            self.circuit_breaker['failure_count'] = 0
            self.ten_env.log_info("KEYPOINT: Circuit breaker reset to CLOSED state")

    def _record_failure(self):
        """Record failed operation"""
        import time

        self.circuit_breaker['failure_count'] += 1
        self.circuit_breaker['last_failure_time'] = time.time()

        if self.circuit_breaker['failure_count'] >= self.circuit_breaker['failure_threshold']:
            self.circuit_breaker['state'] = 'OPEN'
            self.ten_env.log_error(f"KEYPOINT: Circuit breaker OPEN - too many failures ({self.circuit_breaker['failure_count']})")

    def synthesize_audio_sample_rate(self) -> int:
        """Return the audio sample rate"""
        return self.config.sample_rate if self.config else 24000

    def synthesize_audio_channels(self) -> int:
        """Return the number of audio channels (mono)"""
        return 1

    def synthesize_audio_sample_width(self) -> int:
        """Return the sample width in bytes (16-bit PCM)"""
        return 2

    async def on_data(self, ten_env: AsyncTenEnv, data):
        """Override on_data to handle flush properly"""
        try:
            self.ten_env.log_info(f"KEYPOINT: on_data called with data_name: {data.get_name()}")
            data_name = data.get_name()

            if data_name == "tts_flush":
                # Set flush flag and store flush data for later processing
                self.flush_requested = True
                self.pending_flush_data = data
                self.pending_flush_ten_env = ten_env
                self.ten_env.log_info("KEYPOINT: Flush requested - will send tts_flush_end after tts_audio_end")
                # Do NOT call base class yet - we will call it after tts_audio_end
                return
            else:
                # Let parent class handle other data
                await super().on_data(ten_env, data)

        except Exception as e:
            self.ten_env.log_error(f"Error handling data: {str(e)}")

    def _is_valid_text(self, text: str) -> bool:
        """Check if text is valid for TTS synthesis"""
        if not text:
            return False
        
        # Remove whitespace and check if anything remains
        stripped_text = text.strip()
        if not stripped_text:
            return False
        
        import unicodedata
        
        # Count different types of characters
        letter_count = 0
        emoji_count = 0
        symbol_count = 0
        
        for char in text:
            category = unicodedata.category(char)
            if category.startswith('L'):  # Letter
                letter_count += 1
            elif category.startswith('S'):  # Symbol (includes emojis)
                symbol_count += 1
            elif category == 'So':  # Other symbols (includes many emojis)
                emoji_count += 1
        
        # If text is mostly emojis or symbols, it's invalid
        if emoji_count > 0 and letter_count == 0:
            return False
        
        if symbol_count > letter_count and letter_count < 3:
            return False
        
        # Check for math formulas - be more sensitive
        math_chars = '±√²³¹₂₃₄₅₆₇₈₉₀×÷∞∑∏∫∂∆∇∈∉∪∩⊂⊃⊆⊇∧∨¬→←↔≡≠≤≥≈∝∴∵°='
        math_count = sum(1 for char in text if char in math_chars)
        
        # Also count parentheses and operators as potential math indicators
        math_operators = '()[]{}+-*/'
        operator_count = sum(1 for char in text if char in math_operators)
        
        total_math_indicators = math_count + operator_count
        
        # If we have significant math indicators relative to letters, it's likely a formula
        if total_math_indicators >= letter_count * 0.5 and math_count > 0:
            return False
        
        # If we have some letters, it's probably valid
        if letter_count > 0:
            return True
        
        return False

