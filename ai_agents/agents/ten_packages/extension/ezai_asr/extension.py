from datetime import datetime
import json
import os
import struct
import websockets


from typing_extensions import override
from typing import Callable, Dict
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
from .config import ASRConfig
from ten_ai_base.dumper import Dumper
from .reconnect_manager import ReconnectManager


class CustomWebSocketClient:
    def __init__(self, url: str):
        self.url = url
        self.websocket = None
        self.connected = False
        self.event_handlers: Dict[str, Callable] = {}
        self.listen_task = None

    def on(self, event: str, handler: Callable):
        """Register event handler"""
        self.event_handlers[event] = handler

    async def start(self) -> bool:
        """Start WebSocket connection"""
        try:
            self.websocket = await websockets.connect(self.url)
            self.connected = True

            # Trigger Open event
            if "open" in self.event_handlers:
                await self.event_handlers["open"](
                    self, {"type": "connection_opened"}
                )

            # Start listening for messages
            self.listen_task = asyncio.create_task(self._listen_messages())
            return True

        except Exception as e:
            if "error" in self.event_handlers:
                await self.event_handlers["error"](self, {"error": str(e)})
            return False

    async def _listen_messages(self):
        """Listen for WebSocket messages"""
        try:
            async for message in self.websocket:
                try:
                    # Parse JSON message
                    message_obj = json.loads(message)

                    # Trigger Transcript event (adjust according to your message format)
                    if "transcript" in self.event_handlers:
                        await self.event_handlers["transcript"](
                            self, message_obj
                        )
                except json.JSONDecodeError:
                    # If not JSON, it may be binary data
                    pass

        except websockets.exceptions.ConnectionClosed:
            self.connected = False
            # Trigger Close event
            if "close" in self.event_handlers:
                await self.event_handlers["close"](
                    self, {"reason": "connection_closed"}
                )

        except Exception as e:
            self.connected = False
            # Trigger Error event
            if "error" in self.event_handlers:
                await self.event_handlers["error"](self, {"error": str(e)})

    async def send(self, data: bytes):
        """Send data"""
        if self.websocket and self.connected:
            await self.websocket.send(data)

    async def finalize(self):
        """Finalize connection (optional, depending on your needs)"""
        # Actively trigger the transcript event with final=True
        if "transcript" in self.event_handlers:
            message = {"type": "fullSentence", "text": "<END>", "final": True}
            await self.event_handlers["transcript"](self, message)

    async def finish(self):
        """Close connection"""
        if self.listen_task:
            self.listen_task.cancel()

        if self.websocket:
            await self.websocket.close()
            self.websocket = None
            self.connected = False


class EzaiAsrExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.connected: bool = False
        self.client: CustomWebSocketClient | None = None
        self.config: ASRConfig | None = None
        self.audio_dumper: Dumper | None = None
        self.sent_user_audio_duration_ms_before_last_reset: int = 0
        self.last_finalize_timestamp: int = 0

        # Reconnection manager with retry limits and backoff strategy
        self.reconnect_manager: ReconnectManager | None = None

    @override
    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)
        if self.audio_dumper:
            await self.audio_dumper.stop()
            self.audio_dumper = None

    @override
    def vendor(self) -> str:
        """Get the name of the ASR vendor."""
        return "ezai"

    @override
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)

        # Initialize reconnection manager
        self.reconnect_manager = ReconnectManager(logger=ten_env)

        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = ASRConfig.model_validate_json(config_json)
            self.config.update(self.config.params)
            ten_env.log_info(
                f"KEYPOINT vendor_config: {self.config.to_json(sensitive_handling=True)}",
                category=LOG_CATEGORY_KEY_POINT,
            )

            if self.config.dump:
                dump_file_path = os.path.join(
                    self.config.dump_path, DUMP_FILE_NAME
                )
                self.audio_dumper = Dumper(dump_file_path)
        except Exception as e:
            ten_env.log_error(f"invalid property: {e}")
            self.config = ASRConfig.model_validate_json("{}")
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

            # Use custom WebSocket client
            self.client = CustomWebSocketClient(
                self.config.url + f"?token={self.config.token}"
            )

            if self.audio_dumper:
                await self.audio_dumper.start()

            # Register event handlers
            await self._register_custom_event_handlers()

            # Start connection
            result = await self.client.start()
            if not result:
                self.ten_env.log_error("failed to connect to custom websocket")
                await self.send_asr_error(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message="failed to connect to custom websocket",
                    )
                )
                asyncio.create_task(self._handle_reconnect())
            else:
                self.ten_env.log_info("start_connection completed")

        except Exception as e:
            self.ten_env.log_error(f"KEYPOINT start_connection failed: {e}")
            await self.send_asr_error(
                ModuleError(
                    module=MODULE_NAME_ASR,
                    code=ModuleErrorCode.FATAL_ERROR.value,
                    message=str(e),
                ),
            )

    async def _register_custom_event_handlers(self):
        """Register custom WebSocket event handlers"""
        assert self.client is not None
        self.client.on("open", self._custom_event_handler_on_open)
        self.client.on("close", self._custom_event_handler_on_close)
        self.client.on("transcript", self._custom_event_handler_on_transcript)
        self.client.on("error", self._custom_event_handler_on_error)

    # Event handlers
    async def _custom_event_handler_on_open(self, _, event):
        """Handle connection open event"""
        self.ten_env.log_info(
            f"vendor_status_changed: on_open event: {event}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.sent_user_audio_duration_ms_before_last_reset += (
            self.audio_timeline.get_total_user_audio_duration()
        )
        self.audio_timeline.reset()
        self.connected = True

        if self.reconnect_manager:
            self.reconnect_manager.mark_connection_successful()

    async def _custom_event_handler_on_close(self, *args, **kwargs):
        """Handle connection close event"""
        self.ten_env.log_info(
            f"vendor_status_changed: on_close, args: {args}, kwargs: {kwargs}",
            category=LOG_CATEGORY_VENDOR,
        )
        self.connected = False

        if not self.stopped:
            self.ten_env.log_warn(
                "WebSocket connection closed unexpectedly. Reconnecting..."
            )
            await self._handle_reconnect()

    async def _custom_event_handler_on_transcript(self, _, message_obj):
        """Handle transcript result event"""
        assert self.config is not None

        # Process according to your message format
        if message_obj.get("type") == "fullSentence":
            sentence = message_obj.get("text", "")
            if sentence:
                await self._handle_asr_result(
                    text=sentence,
                    final=True,
                    language=self.config.language,
                )
        elif message_obj.get("type") == "realtime":
            sentence = message_obj.get("text", "")
            if sentence:
                await self._handle_asr_result(
                    text=sentence,
                    final=False,
                    language=self.config.language,
                )
        elif message_obj.get("status") == "error":
            await self._custom_event_handler_on_error(_, message_obj)

    async def _custom_event_handler_on_error(self, _, error):
        """Handle error event"""
        self.ten_env.log_error(
            f"vendor_error: {error}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self.send_asr_error(
            ModuleError(
                module=MODULE_NAME_ASR,
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=str(error),
            )
        )

    async def _handle_message(self, message: dict) -> None:
        """Handle message from WebSocket server"""
        try:
            # Try to parse JSON response
            # response_data = json.loads(message)
            response_data = message
            self.ten_env.log_info(f"{response_data}")

            # Filter out binary info, keep only important ASR result info
            filtered_response = {}
            for key, value in response_data.items():
                # Skip base64 encoded audio data and other large binary fields
                if key in [
                    "audio_bytes_base64",
                    "audio_data",
                    "binary_data",
                ] or (isinstance(value, str) and len(value) > 100):
                    filtered_response[key] = (
                        f"[BINARY_DATA_LENGTH_{len(str(value))}]"
                    )
                else:
                    filtered_response[key] = value

            self.ten_env.log_info(f"Received ASR response: {filtered_response}")

            # Handle fullSentence type response
            if response_data.get("type") == "fullSentence":
                sentence = response_data.get("text", "")

                if len(sentence) == 0:
                    self.ten_env.log_debug(
                        "Received fullSentence but text is empty"
                    )
                    return

                # fullSentence is usually the final result
                is_final = True

                self.ten_env.log_info(
                    f"custom ASR got fullSentence: [{sentence}], is_final: {is_final}, stream_id: {self.stream_id}"
                )

                await self._handle_asr_result(
                    sentence,
                    final=is_final,
                )
            elif response_data.get("type") == "realtime":
                sentence = response_data.get("text", "")
                is_final = False

                self.ten_env.log_info(
                    f"custom ASR got realtime: [{sentence}], is_final: {is_final}, stream_id: {self.stream_id}"
                )

                await self._handle_asr_result(
                    sentence,
                    final=is_final,
                )

            # Extract text according to your ASR response format (keep original logic as backup)
            elif "msg" in response_data and response_data.get("code") == 1000:
                sentence = response_data["msg"]

                if len(sentence) == 0:
                    return

                # Determine if it is the final result (adjust according to your API spec)
                is_final = response_data.get("is_final", True)

                self.ten_env.log_info(
                    f"custom ASR got sentence: [{sentence}], is_final: {is_final}, stream_id: {self.stream_id}"
                )

                await self._handle_asr_result(text=sentence, final=is_final)

            # If it is just a status message (e.g. recording_stop, stop_turn_detection), no special handling needed
            elif response_data.get("type") in [
                "recording_stop",
                "stop_turn_detection",
                "transcription_start",
            ]:
                self.ten_env.log_info(
                    f"ASR status message: {response_data.get('type')}"
                )

        except json.JSONDecodeError:
            self.ten_env.log_warn(f"Received non-JSON message: {message}")
        except Exception as e:
            self.ten_env.log_error(f"Error handling message: {e}")

    @override
    async def finalize(self, session_id: str | None) -> None:
        assert self.config is not None

        self.last_finalize_timestamp = int(datetime.now().timestamp() * 1000)
        self.ten_env.log_info(
            f"vendor_cmd: finalize start at {self.last_finalize_timestamp}",
            category=LOG_CATEGORY_VENDOR,
        )
        await self._handle_finalize_api()

    async def _handle_asr_result(
        self,
        text: str,
        final: bool,
        start_ms: int = 0,
        duration_ms: int = 0,
        language: str = "",
    ):
        """Handle the ASR result from EZAI ASR."""
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
        Connection success is determined by the _custom_event_handler_on_open callback.

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
        """Stop the WebSocket connection."""
        try:
            if self.client is not None:
                await self.client.finish()
                self.client = None
                self.connected = False
                self.ten_env.log_info("websocket connection stopped")
        except Exception as e:
            self.ten_env.log_error(f"Error stopping websocket connection: {e}")

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

        # Prepare metadata
        metadata = {
            "sampleRate": self.config.sample_rate,
            "channels": self.config.channels,
            "sampwidth": self.config.sampwidth,
            "language": self.config.language,
        }
        metadata_json = json.dumps(metadata)
        metadata_length = len(metadata_json)

        # Pack data: metadata length + metadata + audio data
        message = (
            struct.pack("<I", metadata_length)
            + metadata_json.encode("utf-8")
            + bytes(buf)
        )

        await self.client.send(message)
        frame.unlock_buf(buf)

        return True
