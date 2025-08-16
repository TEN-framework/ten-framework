#
#
# Agora Real Time Engagement
# Created by XinHui Li in 2024-07.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
from .config import ElevenLabsTTS2Config
import websockets
import json
import base64
import asyncio
from ten_ai_base.message import (
    ModuleVendorException,
    ModuleErrorVendorInfo,
    ModuleError,
    ModuleErrorCode,
    ModuleType,
)
from ten_runtime import (
    AsyncTenEnv,
)
from typing import Callable, Awaitable


class ElevenLabsTTS2:
    def __init__(
        self,
        config: ElevenLabsTTS2Config,
        ten_env: AsyncTenEnv,
        error_callback: Callable[[str, ModuleError], Awaitable[None]] = None,
    ) -> None:
        self.config = config
        self.ws = None
        self.ws_recv_task = None
        self.ws_send_task = None
        self.uri = f"wss://api.elevenlabs.io/v1/text-to-speech/{self.config.voice_id}/stream-input?model_id={self.config.model_id}"
        self.text_input_queue = asyncio.Queue()
        self.audio_data_queue = asyncio.Queue()
        self.is_connected = False
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 5
        self.reconnect_delay = 1.0  # Initial reconnection delay 1 second
        self.max_reconnect_delay = 30.0  # Maximum reconnection delay 30 seconds
        self.ten_env = ten_env
        self.error_callback = error_callback

    async def start_connection(self):
        """Start WebSocket connection"""
        try:
            self.ten_env.log_info(
                f"Starting WebSocket connection to {self.uri}"
            )
            self.ws = await websockets.connect(
                self.uri, ping_interval=20, ping_timeout=10, close_timeout=10
            )
            self.is_connected = True
            self.reconnect_attempts = 0
            self.reconnect_delay = 1.0

            # Start receive and send tasks
            self.ws_recv_task = asyncio.create_task(self.ws_recv_loop())
            self.ws_send_task = asyncio.create_task(
                self.text_to_speech_ws_streaming()
            )

            # Send initialization message
            await self.ws.send(
                json.dumps(
                    {
                        "text": " ",
                        "voice_settings": {
                            "stability": self.config.stability,
                            "similarity_boost": self.config.similarity_boost,
                            "use_speaker_boost": self.config.speaker_boost,
                        },
                        "xi_api_key": self.config.api_key,
                    }
                )
            )

            self.ten_env.log_info(
                "WebSocket connection established successfully"
            )

        except Exception as e:
            self.ten_env.log_error(
                f"Failed to establish WebSocket connection: {e}"
            )
            self.is_connected = False

            error_info = ModuleErrorVendorInfo(
                vendor="elevenlabs", code="CONNECTION_FAILED", message=str(e)
            )

            if self.error_callback:
                module_error = ModuleError(
                    message=f"Failed to connect to ElevenLabs WebSocket: {str(e)}",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=error_info,
                )
                await self.error_callback("", module_error)
            else:
                raise ModuleVendorException(error_info) from e

    async def ws_recv_loop(self):
        """WebSocket receive loop, process audio data"""
        while self.is_connected:
            try:
                if not self.ws or self.ws.state.name == "CLOSED":
                    self.ten_env.log_warning(
                        "WebSocket is closed, attempting to reconnect..."
                    )
                    await self._handle_reconnection()
                    continue

                message = await self.ws.recv()
                data = json.loads(message)

                isFinal = False
                audio_data = None
                text = ""
                if data.get("alignment"):
                    alignment = data.get("alignment")
                    if alignment.get("chars"):
                        chars = alignment.get("chars")
                        for char in chars:
                            text += char
                    self.ten_env.log_debug(
                        f"Received alignment from WebSocket: {text}"
                    )
                if data.get("isFinal"):
                    isFinal = data.get("isFinal")

                if data.get("audio"):
                    audio_data = base64.b64decode(data["audio"])

                await self.audio_data_queue.put([audio_data, isFinal, text])
                if isFinal:
                    self.ten_env.log_info(
                        "Received final message from WebSocket"
                    )
                    return
                if data.get("error"):
                    error_info = ModuleErrorVendorInfo(
                        vendor="elevenlabs",
                        code=str(data.get("code", 0)),
                        message=data.get("error", "Unknown error"),
                    )
                    error_code = ModuleErrorCode.NON_FATAL_ERROR
                    if data.get("code") == 1008:
                        error_code = ModuleErrorCode.FATAL_ERROR

                    if self.error_callback:
                        # Send error using callback function
                        module_error = ModuleError(
                            message=data["error"],
                            module=ModuleType.TTS,
                            code=error_code,
                            vendor_info=error_info,
                        )
                        await self.error_callback("", module_error)
                    else:
                        # If no callback function, raise exception
                        raise ModuleVendorException(error_info)

            except websockets.exceptions.ConnectionClosed as e:
                self.ten_env.log_warning(f"WebSocket connection closed: {e}")
                await self._handle_reconnection()
            except websockets.exceptions.WebSocketException as e:
                self.ten_env.log_error(f"WebSocket error: {e}")
                await self._handle_reconnection()
            except json.JSONDecodeError as e:
                self.ten_env.log_error(
                    f"Failed to parse WebSocket message: {e}"
                )
                continue
            except asyncio.CancelledError:
                self.ten_env.log_info("ws_recv_loop task cancelled")
                break
            except Exception as e:
                self.ten_env.log_error(f"Unexpected error in ws_recv_loop: {e}")
                await self._handle_reconnection()

    async def text_to_speech_ws_streaming(self):
        """WebSocket send loop, process text input"""
        while self.is_connected:
            try:
                if not self.ws or self.ws.state.name == "CLOSED":
                    self.ten_env.log_warning(
                        "WebSocket is closed in send loop, waiting for reconnection..."
                    )
                    await asyncio.sleep(0.1)
                    continue

                t = await self.text_input_queue.get()

                if t.text.strip() != "":
                    await self.ws.send(json.dumps({"text": t.text}))
                    self.ten_env.log_debug(
                        f"Sent text to WebSocket: {t.text[:50]}..."
                    )

                if t.text_input_end:
                    await self.ws.send(json.dumps({"text": ""}))
                    self.ten_env.log_debug("Sent end signal to WebSocket")
                    return

            except websockets.exceptions.ConnectionClosed as e:
                self.ten_env.log_warning(
                    f"WebSocket connection closed in send loop: {e}"
                )
                await self._handle_reconnection()
            except websockets.exceptions.WebSocketException as e:
                self.ten_env.log_error(f"WebSocket error in send loop: {e}")
                await self._handle_reconnection()
            except asyncio.CancelledError:
                self.ten_env.log_info(
                    "text_to_speech_ws_streaming task cancelled"
                )
                break
            except Exception as e:
                self.ten_env.log_error(
                    f"Unexpected error in text_to_speech_ws_streaming: {e}"
                )
                await self._handle_reconnection()

    async def _handle_reconnection(self):
        """Handle reconnection logic"""
        if self.reconnect_attempts >= self.max_reconnect_attempts:
            self.ten_env.log_error(
                f"Max reconnection attempts ({self.max_reconnect_attempts}) reached"
            )
            self.is_connected = False

            error_info = ModuleErrorVendorInfo(
                vendor="elevenlabs",
                code="MAX_RECONNECT_ATTEMPTS",
                message="Failed to reconnect after maximum attempts",
            )

            if self.error_callback:
                module_error = ModuleError(
                    message="Max reconnection attempts reached",
                    module=ModuleType.TTS,
                    code=ModuleErrorCode.FATAL_ERROR,
                    vendor_info=error_info,
                )
                await self.error_callback("", module_error)
            else:
                raise ModuleVendorException(error_info)

        self.reconnect_attempts += 1
        self.is_connected = False

        # Cancel existing tasks
        if self.ws_recv_task and not self.ws_recv_task.done():
            self.ws_recv_task.cancel()
            try:
                await self.ws_recv_task
            except asyncio.CancelledError:
                pass
        if self.ws_send_task and not self.ws_send_task.done():
            self.ws_send_task.cancel()
            try:
                await self.ws_send_task
            except asyncio.CancelledError:
                pass

        # Close existing connection
        if self.ws and self.ws.state.name != "CLOSED":
            try:
                await self.ws.close()
            except Exception as e:
                self.ten_env.log_warning(f"Error closing WebSocket: {e}")

        # Wait for reconnection delay
        delay = min(
            self.reconnect_delay * (2 ** (self.reconnect_attempts - 1)),
            self.max_reconnect_delay,
        )
        self.ten_env.log_info(
            f"Attempting to reconnect in {delay} seconds (attempt {self.reconnect_attempts}/{self.max_reconnect_attempts})"
        )
        await asyncio.sleep(delay)

        try:
            await self.start_connection()
            self.ten_env.log_info("Reconnection successful")
        except Exception as e:
            self.ten_env.log_error(f"Reconnection failed: {e}")
            # Continue retrying
            await self._handle_reconnection()

    async def reconnect_connection(self):
        """Manual reconnection method"""
        self.ten_env.log_info("Manual reconnection requested")
        self.reconnect_attempts = 0
        await self._handle_reconnection()

    async def get_synthesized_audio(self):
        """Get synthesized audio data"""
        try:
            return await self.audio_data_queue.get()
        except asyncio.CancelledError:
            self.ten_env.log_info("get_synthesized_audio cancelled")
            raise
        except asyncio.QueueEmpty:
            self.ten_env.log_warning("Audio data queue is empty")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Error getting synthesized audio: {e}")
            raise

    async def handle_flush(self):
        """Handle flush request"""

        try:
            # Clear queue
            while not self.audio_data_queue.empty():
                try:
                    self.audio_data_queue.get_nowait()
                except asyncio.QueueEmpty:
                    break

            while not self.text_input_queue.empty():
                try:
                    self.text_input_queue.get_nowait()
                except asyncio.QueueEmpty:
                    break

            # Re-establish connection
            await self.reconnect_connection()
            self.ten_env.log_info("Flush handling completed")

        except Exception as e:
            self.ten_env.log_error(f"Error handling flush: {e}")
            raise

    async def close_connection(self):
        """Close connection"""
        self.ten_env.log_info("Closing WebSocket connection")
        self.is_connected = False

        # Cancel tasks
        if self.ws_recv_task and not self.ws_recv_task.done():
            self.ws_recv_task.cancel()
            try:
                await self.ws_recv_task
            except asyncio.CancelledError:
                pass
        if self.ws_send_task and not self.ws_send_task.done():
            self.ws_send_task.cancel()
            try:
                await self.ws_send_task
            except asyncio.CancelledError:
                pass

        # Close WebSocket
        if self.ws and self.ws.state.name != "CLOSED":
            try:
                await self.ws.close()
            except Exception as e:
                self.ten_env.log_warning(f"Error closing WebSocket: {e}")

        # Clear queue
        while not self.audio_data_queue.empty():
            try:
                self.audio_data_queue.get_nowait()
            except asyncio.QueueEmpty:
                break
        while not self.text_input_queue.empty():
            try:
                self.text_input_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

        self.ten_env.log_info("WebSocket connection closed")

    def is_connection_healthy(self) -> bool:
        """Check if connection is healthy"""
        return (
            self.is_connected
            and self.ws
            and self.ws.state.name != "CLOSED"
            and self.ws_recv_task
            and not self.ws_recv_task.done()
            and self.ws_send_task
            and not self.ws_send_task.done()
        )

    def _create_error_info(self, error_data: dict) -> ModuleErrorVendorInfo:
        """Create error information"""
        return ModuleErrorVendorInfo(
            vendor="elevenlabs",
            code=error_data.get("code", "UNKNOWN_ERROR"),
            message=error_data.get("error", "Unknown error"),
        )
