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
from typing import Callable, Awaitable, Tuple
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


class ElevenLabsTTS2Synthesizer:
    def __init__(
        self,
        config: ElevenLabsTTS2Config,
        ten_env: AsyncTenEnv,
        error_callback: Callable[[str, ModuleError], Awaitable[None]] = None,
        response_msgs: asyncio.Queue[Tuple[bytes, bool, str]] = None,
    ) -> None:
        self.config = config
        self.ws = None
        self.uri = f"wss://api.elevenlabs.io/v1/text-to-speech/{self.config.voice_id}/stream-input?model_id={self.config.model_id}&output_format=pcm_{self.config.sample_rate}&sync_alignment=true"
        self.text_input_queue = asyncio.Queue()
        self.ten_env = ten_env
        self.error_callback = error_callback
        self.response_msgs = response_msgs

        # Connection management related
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        self.channel_tasks = []
        self._session_started = False

        # Mechanism for waiting for specific events
        self._connection_event = asyncio.Event()
        self._connection_success = False
        self._receive_ready_event = asyncio.Event()

        # Start websocket connection monitoring
        self.websocket_task = asyncio.create_task(self._process_websocket())

    def _process_ws_exception(self, exp) -> None | Exception:
        """Handle websocket connection exceptions and decide whether to reconnect"""
        self.ten_env.log_warn(
            f"Websocket internal error during connecting: {exp}."
        )
        self._connect_exp_cnt += 1
        if self._connect_exp_cnt > 5:  # MAX_RETRY_TIMES_FOR_TRANSPORT
            self.ten_env.log_error(f"Max retries (5) exceeded: {str(exp)}")
            return exp
        return None  # Return None to continue reconnection

    async def _process_websocket(self) -> None:
        """Main websocket connection monitoring and reconnection logic"""
        try:
            self.ten_env.log_info("Starting websocket connection process")
            # Use websockets.connect's automatic reconnection mechanism
            async for ws in websockets.connect(
                uri=self.uri,
                ping_interval=20,
                ping_timeout=10,
                close_timeout=10,
                process_exception=self._process_ws_exception,
            ):
                self.ws = ws
                try:
                    self.ten_env.log_info("Websocket connected successfully")
                    if self._session_closing:
                        self.ten_env.log_info("Session is closing, break.")
                        return

                    # Start send and receive tasks
                    self.channel_tasks = [
                        asyncio.create_task(self._send_loop(ws)),
                        asyncio.create_task(self._receive_loop(ws)),
                    ]

                    # Wait for receive loop to be ready before establishing connection
                    await self._receive_ready_event.wait()
                    await self.start_connection()

                    await self._await_channel_tasks()

                except websockets.ConnectionClosed as e:
                    self.ten_env.log_info(f"Websocket connection closed: {e}.")
                    if not self._session_closing:
                        self.ten_env.log_info(
                            "Websocket connection closed, will reconnect."
                        )

                        # Cancel all channel tasks
                        for task in self.channel_tasks:
                            task.cancel()
                        await self._await_channel_tasks()

                        # Reset all event states
                        self._receive_ready_event.clear()
                        self._connection_event.clear()
                        self._connection_success = False
                        self._session_started = False

                        # Reset connection exception counter
                        self._connect_exp_cnt = 0
                        continue

        except Exception as e:
            self.ten_env.log_error(f"Exception in websocket process: {e}")
        finally:
            if self.ws:
                await self.ws.close()
            self.ten_env.log_info("Websocket connection process ended.")

    async def _await_channel_tasks(self) -> None:
        """Wait for channel tasks to complete"""
        if not self.channel_tasks:
            return

        (done, pending) = await asyncio.wait(
            self.channel_tasks,
            return_when=asyncio.FIRST_EXCEPTION,
        )
        self.ten_env.log_info("Channel tasks finished.")

        self.channel_tasks.clear()

        # Cancel remaining tasks
        for task in pending:
            task.cancel()

        # Check for exceptions
        for task in done:
            exp = task.exception()
            if exp and not isinstance(exp, asyncio.CancelledError):
                raise exp

    async def _send_loop(self, ws: ClientConnection) -> None:
        """Text sending loop"""
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
            self.ten_env.log_info(" websocket connection established")

            while not self._session_closing:
                # Get text to send from queue
                try:
                    text_data = await asyncio.wait_for(
                        self.text_input_queue.get(), timeout=18
                    )
                except asyncio.TimeoutError:
                    # timeout error, send empty text to keep the connection alive
                    self.ten_env.log_debug(
                        "No new text input, sending space text to keep alive."
                    )
                    text_data = {"text": " ", "flush": True}
                if text_data is None:
                    self.ten_env.log_debug(
                        "Received None from queue, ending send loop"
                    )
                    continue

                if text_data.text.strip() != "":
                    await ws.send(
                        json.dumps({"text": text_data.text, "flush": True})
                    )
                    self.ten_env.log_debug(
                        f"Sent text to WebSocket: {text_data.text[:50]}..."
                    )

                if text_data.text_input_end == True:
                    await ws.send(json.dumps({"text": ""}))
                    self.ten_env.log_debug("Sent end signal to WebSocket")
                    return
            self.ten_env.log_info("websocket connection closed")

        except asyncio.CancelledError:
            self.ten_env.log_info("send_loop task cancelled")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Exception in send_loop: {e}")
            raise e

    async def _receive_loop(self, ws: ClientConnection) -> None:
        """Message receiving loop"""
        try:
            # Mark receive loop as ready
            self._receive_ready_event.set()

            async for message in ws:
                if self._session_closing:
                    self.ten_env.log_warn(
                        "Session is closing, break receive loop."
                    )
                    break

                try:
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

                    if self.response_msgs is not None:
                        await self.response_msgs.put(
                            (audio_data, isFinal, text)
                        )

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
            self.ten_env.log_debug("receive_loop cancelled")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Exception in receive_loop: {e}")
            raise e

    async def start_connection(self):
        """Establish connection"""
        # Reset connection event
        self._connection_event.clear()
        self._connection_success = False

        # Connection is established when websocket is connected
        self._connection_success = True
        self._connection_event.set()

    async def send_text(self, text_data):
        """Send text (external interface)"""
        await self.text_input_queue.put(text_data)

    def cancel(self) -> None:
        """Cancel current connection, used for flush scenarios"""
        self.ten_env.log_info("Cancelling the request.")

        # The websocket connection might be not established yet, if so, using
        # this flag to close the connection directly.
        self._session_closing = True

        # Cancel websocket task
        if self.websocket_task:
            self.websocket_task.cancel()

        # Note that the websocket connection might not be established yet
        # (i.e., self.channel_tasks is empty).
        for task in self.channel_tasks:
            task.cancel()

        # Clear all queues to prevent old data from being processed
        self._clear_queues()

        # We do not wait the websocket_task to be completed, as the duration
        # of closing the websocket might be more than 10 seconds by default.
        #
        # After the sender/receiver tasks are completed, `self._process_websocket()`
        # should be quit soon, and then `close()` will be called on the
        # websocket connection at exit of function. So the websocket connection
        # will be closed eventually.

    def _clear_queues(self) -> None:
        """Clear all queues to prevent old data from being processed"""
        # Clear text queue
        while not self.text_input_queue.empty():
            try:
                self.text_input_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

        # Clear response messages queue
        if self.response_msgs:
            while not self.response_msgs.empty():
                try:
                    self.response_msgs.get_nowait()
                except asyncio.QueueEmpty:
                    break

        self.ten_env.log_info("All queues cleared during cancel")

    async def close(self):
        self.ten_env.log_info("Closing ElevenLabsTTS2Synthesizer")

        # Set closing flag
        self._session_closing = True

        # Send end signal to text queue
        await self.text_input_queue.put(None)

        # Cancel websocket task
        if self.websocket_task:
            self.websocket_task.cancel()
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                pass

        # Close websocket connection
        if self.ws:
            await self.ws.close()
            self.ws = None
        self.response_msgs = None


class ElevenLabsTTS2Client:
    def __init__(
        self,
        config: ElevenLabsTTS2Config,
        ten_env: AsyncTenEnv,
        error_callback: Callable[[str, ModuleError], Awaitable[None]] = None,
        response_msgs: asyncio.Queue[Tuple[bytes, bool, str]] = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.error_callback = error_callback
        self.response_msgs = response_msgs

        # Current active synthesizer
        self.synthesizer: ElevenLabsTTS2Synthesizer = self._create_synthesizer()

        # List of synthesizers to be cleaned up
        self.cancelled_synthesizers = []

        # Cleanup task
        self.cleanup_task = asyncio.create_task(
            self._cleanup_cancelled_synthesizers()
        )

    def _create_synthesizer(self) -> ElevenLabsTTS2Synthesizer:
        """Create new synthesizer instance"""
        return ElevenLabsTTS2Synthesizer(
            self.config, self.ten_env, self.error_callback, self.response_msgs
        )

    async def _cleanup_cancelled_synthesizers(self) -> None:
        """Periodically clean up completed cancelled synthesizers"""
        while True:
            try:
                for synthesizer in self.cancelled_synthesizers[:]:
                    if (
                        synthesizer.websocket_task
                        and synthesizer.websocket_task.done()
                    ):
                        self.ten_env.log_info(
                            f"Cleaning up cancelled synthesizer {id(synthesizer)}"
                        )
                        self.cancelled_synthesizers.remove(synthesizer)

                await asyncio.sleep(5.0)  # Check every 5 seconds
            except Exception as e:
                self.ten_env.log_error(f"Error in cleanup task: {e}")
                await asyncio.sleep(5.0)

    def cancel(self) -> None:
        """Cancel current synthesizer and create new synthesizer"""
        self.ten_env.log_info(
            "Cancelling current synthesizer and creating new one"
        )

        # Clear response messages queue to prevent old data from being processed
        if self.response_msgs:
            while not self.response_msgs.empty():
                try:
                    self.response_msgs.get_nowait()
                except asyncio.QueueEmpty:
                    break
            self.ten_env.log_info(
                "Response messages queue cleared during cancel"
            )

        # Move current synthesizer to cleanup list
        if self.synthesizer:
            self.cancelled_synthesizers.append(self.synthesizer)
            self.synthesizer.cancel()

        # Create new synthesizer
        self.synthesizer = self._create_synthesizer()
        self.ten_env.log_info("New synthesizer created successfully")

    async def send_text(self, text_data):
        """Send text"""
        await self.synthesizer.send_text(text_data)

    async def close(self):
        """Close client"""
        self.ten_env.log_info("Closing ElevenLabsTTS2Client")

        # Cancel cleanup task
        if self.cleanup_task:
            self.cleanup_task.cancel()
            try:
                await self.cleanup_task
            except asyncio.CancelledError:
                pass

        # Close current synthesizer
        if self.synthesizer:
            await self.synthesizer.close()

        # Close all cancelled synthesizers
        for synthesizer in self.cancelled_synthesizers:
            try:
                await synthesizer.close()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error closing cancelled synthesizer: {e}"
                )

        self.cancelled_synthesizers.clear()
        self.ten_env.log_info("ElevenLabsTTS2Client closed")


# Backward compatibility - keep the old class name for existing code
class ElevenLabsTTS2:
    def __init__(
        self,
        config: ElevenLabsTTS2Config,
        ten_env: AsyncTenEnv,
        error_callback: Callable[[str, ModuleError], Awaitable[None]] = None,
    ) -> None:
        self.client = ElevenLabsTTS2Client(
            config, ten_env, error_callback, None
        )
        self.text_input_queue = self.client.synthesizer.text_input_queue
        self.audio_data_queue = asyncio.Queue()

    async def get_synthesized_audio(self):
        """Get synthesized audio data"""
        try:
            return await self.audio_data_queue.get()
        except asyncio.CancelledError:
            self.client.ten_env.log_info("get_synthesized_audio cancelled")
            raise
        except QueueEmpty:
            self.client.ten_env.log_error("Audio data queue is empty")
            raise
        except Exception as e:
            self.client.ten_env.log_error(
                f"Error getting synthesized audio: {e}"
            )
            raise

    async def handle_flush(self):
        """Handle flush request - immediately disconnect and re-establish connection"""
        self.client.cancel()

    async def close_connection(self):
        """Close connection"""
        await self.client.close()

    def is_connection_healthy(self) -> bool:
        """Check if connection is healthy"""
        return (
            self.client.synthesizer.ws
            and self.client.synthesizer.ws.state.name != "CLOSED"
            and self.client.synthesizer.websocket_task
            and not self.client.synthesizer.websocket_task.done()
        )

    def _create_error_info(self, error_data: dict) -> ModuleErrorVendorInfo:
        """Create error information"""
        return ModuleErrorVendorInfo(
            vendor="elevenlabs",
            code=error_data.get("code", "UNKNOWN_ERROR"),
            message=error_data.get("error", "Unknown error"),
        )
