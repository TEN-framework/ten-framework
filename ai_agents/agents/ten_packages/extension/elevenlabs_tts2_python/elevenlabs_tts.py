#
#
# Agora Real Time Engagement
# Created by XinHui Li in 2024-07.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
import asyncio
import json
import base64
import websockets
from typing import Callable, Awaitable
from websockets.asyncio.client import ClientConnection
from asyncio import QueueEmpty

from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
    ModuleVendorException,
)
from ten_ai_base import ModuleType
from ten_runtime import AsyncTenEnv
from .config import ElevenLabsTTS2Config


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
        self.uri = f"wss://api.elevenlabs.io/v1/text-to-speech/{self.config.voice_id}/stream-input?model_id={self.config.model_id}&output_format=pcm_{self.config.sample_rate}&sync_alignment=true"
        self.text_input_queue = asyncio.Queue()
        self.audio_data_queue = asyncio.Queue()
        self.is_connected = False
        self.reconnect_attempts = 0
        self.max_reconnect_attempts = 5
        self.reconnect_delay = 1.0  # Initial reconnection delay 1 second
        self.max_reconnect_delay = 30.0  # Maximum reconnection delay 30 seconds
        self.ten_env = ten_env
        self.error_callback = error_callback

        # New: Unified state management
        self._session_closing = False
        self._connection_lock = asyncio.Lock()
        self._reconnect_requested = False
        self._flush_requested = False  # New: flush request flag
        self._main_loop_task = None
        self._channel_tasks = []

    async def start_connection(self):
        """Start the main connection loop"""
        if self._main_loop_task and not self._main_loop_task.done():
            self.ten_env.log_error("Main loop task already running")
            return

        self._main_loop_task = asyncio.create_task(self._main_connection_loop())
        self.ten_env.log_info("Main connection loop started")

    async def _main_connection_loop(self):
        """Main connection loop that handles reconnection automatically"""
        while not self._session_closing:
            try:
                self.ten_env.log_info(
                    f"Starting WebSocket connection to {self.uri}"
                )

                # Use websockets.connect infinite loop for automatic reconnection
                async for ws in websockets.connect(
                    self.uri,
                    ping_interval=20,
                    ping_timeout=10,
                    close_timeout=10,
                ):
                    self.ws = ws
                    self.is_connected = True
                    self.reconnect_attempts = 0
                    self.reconnect_delay = 1.0

                    self.ten_env.log_info(
                        "WebSocket connection established successfully"
                    )

                    # Check if session closing is requested
                    if self._session_closing:
                        self.ten_env.log_info(
                            "Session closing requested, breaking connection loop"
                        )
                        break

                    # Start send and receive tasks
                    self._channel_tasks = [
                        asyncio.create_task(self._ws_recv_loop(ws)),
                        asyncio.create_task(self._ws_send_loop(ws)),
                    ]

                    # Wait for tasks to complete or error
                    await self._await_channel_tasks()

                    # Check if connection was closed due to flush request
                    if self._flush_requested:
                        self.ten_env.log_info(
                            "Flush detected - connection was intentionally closed"
                        )
                        # Reset flush flag
                        self._flush_requested = False
                        # Continue loop to re-establish connection
                        continue

                    # If we reach here, connection was closed and needs reconnection
                    if not self._session_closing:
                        self.ten_env.log_info(
                            "WebSocket connection closed, reconnecting..."
                        )
                        # Clean up pending text for re-sending on reconnection
                        await self._handle_connection_loss()

            except websockets.ConnectionClosed as e:
                self.ten_env.log_error(f"WebSocket connection closed: {e}")
                if not self._session_closing:
                    await self._handle_connection_loss()

            except Exception as e:
                self.ten_env.log_error(
                    f"Exception in main connection loop: {e}"
                )
                if not self._session_closing:
                    await self._handle_connection_loss()

        self.ten_env.log_info("Main connection loop ended")

    async def _await_channel_tasks(self):
        """Wait for channel tasks to complete or error"""
        if not self._channel_tasks:
            return

        try:
            done, pending = await asyncio.wait(
                self._channel_tasks, return_when=asyncio.FIRST_EXCEPTION
            )

            # Cancel remaining tasks
            for task in pending:
                task.cancel()

            # Check for exceptions
            for task in done:
                exp = task.exception()
                if exp and not isinstance(exp, asyncio.CancelledError):
                    self.ten_env.log_error(f"Channel task exception: {exp}")
                    raise exp

            # Check if tasks were cancelled due to flush request
            if self._flush_requested:
                self.ten_env.log_info(
                    "Flush detected in channel tasks - tasks were cancelled intentionally"
                )

        except asyncio.CancelledError:
            # Main task cancelled, cancel all child tasks
            for task in self._channel_tasks:
                task.cancel()
            raise
        finally:
            self._channel_tasks.clear()

    async def _handle_connection_loss(self):
        """Handle connection loss"""
        self.is_connected = False

        # Cancel existing tasks
        for task in self._channel_tasks:
            if not task.done():
                task.cancel()

        # Wait for tasks to complete
        try:
            await asyncio.wait(self._channel_tasks, timeout=5.0)
        except asyncio.TimeoutError:
            self.ten_env.log_error(
                "Timeout waiting for channel tasks to cancel"
            )

        self._channel_tasks.clear()

        # Close WebSocket connection
        if self.ws and self.ws.state.name != "CLOSED":
            try:
                await self.ws.close()
            except Exception as e:
                self.ten_env.log_error(f"Error closing WebSocket: {e}")

    async def _ws_recv_loop(self, ws: ClientConnection):
        """WebSocket receive loop"""
        try:
            while (
                self.is_connected
                and not self._session_closing
                and not self._flush_requested
            ):
                try:
                    message = await ws.recv()
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
                            module_error = ModuleError(
                                message=data["error"],
                                module=ModuleType.TTS,
                                code=error_code,
                                vendor_info=error_info,
                            )
                            await self.error_callback("", module_error)
                        else:
                            raise ModuleVendorException(error_info)

                except json.JSONDecodeError as e:
                    self.ten_env.log_error(
                        f"Failed to parse WebSocket message: {e}"
                    )
                    continue

        except asyncio.CancelledError:
            self.ten_env.log_info("ws_recv_loop task cancelled")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Unexpected error in ws_recv_loop: {e}")
            raise

    async def _ws_send_loop(self, ws: ClientConnection):
        """WebSocket send loop"""
        try:
            # Send initialization message
            await ws.send(
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

            while (
                self.is_connected
                and not self._session_closing
                and not self._flush_requested
            ):
                try:
                    t = await self.text_input_queue.get()

                    if t.text.strip() != "":
                        await ws.send(json.dumps({"text": t.text}))
                        self.ten_env.log_debug(
                            f"Sent text to WebSocket: {t.text[:50]}..."
                        )

                    if t.text_input_end:
                        await ws.send(json.dumps({"text": ""}))
                        self.ten_env.log_debug("Sent end signal to WebSocket")
                        return

                except asyncio.CancelledError:
                    self.ten_env.log_info("ws_send_loop task cancelled")
                    raise

        except Exception as e:
            self.ten_env.log_error(f"Unexpected error in ws_send_loop: {e}")
            raise

    async def request_reconnect(self):
        """Request reconnection - set flag to let main loop handle reconnection"""
        async with self._connection_lock:
            if not self._reconnect_requested:
                self._reconnect_requested = True
                self.ten_env.log_info("Reconnect requested")

                # Trigger reconnection: close current connection
                if self.ws and self.ws.state.name != "CLOSED":
                    try:
                        await self.ws.close()
                    except Exception as e:
                        self.ten_env.log_error(
                            f"Error closing WebSocket for reconnect: {e}"
                        )

    async def get_synthesized_audio(self):
        """Get synthesized audio data"""
        try:
            return await self.audio_data_queue.get()
        except asyncio.CancelledError:
            self.ten_env.log_info("get_synthesized_audio cancelled")
            raise
        except QueueEmpty:
            self.ten_env.log_error("Audio data queue is empty")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Error getting synthesized audio: {e}")
            raise

    async def handle_flush(self):
        """Handle flush request - immediately disconnect and re-establish connection"""
        try:
            self.ten_env.log_info(
                "Flush requested - immediately disconnecting current connection"
            )

            # Set flush flag
            self._flush_requested = True

            # Clear queues
            while not self.audio_data_queue.empty():
                try:
                    self.audio_data_queue.get_nowait()
                except QueueEmpty:
                    break

            while not self.text_input_queue.empty():
                try:
                    self.text_input_queue.get_nowait()
                except QueueEmpty:
                    break

            # Immediately close current WebSocket connection
            if self.ws and self.ws.state.name != "CLOSED":
                try:
                    await self.ws.close()
                    self.ten_env.log_info(
                        "Current WebSocket connection closed for flush"
                    )
                except Exception as e:
                    self.ten_env.log_error(
                        f"Error closing WebSocket for flush: {e}"
                    )

            # Reset connection state
            self.is_connected = False

            # Cancel current channel tasks
            for task in self._channel_tasks:
                if not task.done():
                    task.cancel()

            # Wait for tasks to complete
            try:
                await asyncio.wait(self._channel_tasks, timeout=2.0)
            except asyncio.TimeoutError:
                self.ten_env.log_warning(
                    "Timeout waiting for channel tasks to cancel during flush"
                )

            self._channel_tasks.clear()

            # Reset flush flag to let main loop re-establish connection
            self._flush_requested = False

            self.ten_env.log_info(
                "Flush handling completed - connection will be re-established by main loop"
            )

        except Exception as e:
            self.ten_env.log_error(f"Error handling flush: {e}")
            raise

    async def close_connection(self):
        """Close connection"""
        self.ten_env.log_info("Closing WebSocket connection")
        self._session_closing = True

        # Cancel main loop task
        if self._main_loop_task and not self._main_loop_task.done():
            self._main_loop_task.cancel()
            try:
                await self._main_loop_task
            except asyncio.CancelledError:
                pass

        # Clear queues
        while not self.audio_data_queue.empty():
            try:
                self.audio_data_queue.get_nowait()
            except QueueEmpty:
                break
        while not self.text_input_queue.empty():
            try:
                self.text_input_queue.get_nowait()
            except QueueEmpty:
                break

        self.ten_env.log_info("WebSocket connection closed")

    def is_connection_healthy(self) -> bool:
        """Check if connection is healthy"""
        return (
            self.is_connected
            and self.ws
            and self.ws.state.name != "CLOSED"
            and self._main_loop_task
            and not self._main_loop_task.done()
        )

    def _create_error_info(self, error_data: dict) -> ModuleErrorVendorInfo:
        """Create error information"""
        return ModuleErrorVendorInfo(
            vendor="elevenlabs",
            code=error_data.get("code", "UNKNOWN_ERROR"),
            message=error_data.get("error", "Unknown error"),
        )
