from abc import abstractmethod
import asyncio
import websockets

import json
from .const import TIMEOUT_CODE
from websockets.protocol import State
from ten_ai_base.timeline import AudioTimeline
from ten_ai_base.const import (
    LOG_CATEGORY_VENDOR,
)
from ten_runtime import (
    AsyncTenEnv,
)


class DeepgramASRRecognitionCallback:
    """WebSocket Speech Recognition Callback Interface"""

    @abstractmethod
    async def on_open(self):
        """Called when connection is established"""

    @abstractmethod
    async def on_result(self, message_data):
        """
        Recognition result callback
        :param message_data: Complete recognition result data
        """

    @abstractmethod
    async def on_error(self, error_msg, error_code=None):
        """Error callback"""

    @abstractmethod
    async def on_close(self):
        """Called when connection is closed"""


class DeepgramASRRecognition:
    """Async WebSocket-based speech recognition class"""

    def __init__(
        self,
        api_key: str,
        audio_timeline: AudioTimeline,
        ten_env: AsyncTenEnv,
        config: dict,
        callback: DeepgramASRRecognitionCallback,
    ):

        self.api_key = api_key
        self.url = config.get("url", "wss://api.deepgram.com/v1/listen")
        self.ten_env = ten_env
        self.audio_timeline = audio_timeline

        self.config = config

        if self.config is None:
            self.config = {}

        self.config["mip_opt_out"] = True

        self.callback = callback

        self.websocket = None
        self.is_started = False
        self._message_task = None

    async def _handle_message(self, message):
        """Handle WebSocket message"""
        try:
            message_data = json.loads(message)

            self.ten_env.log_debug(
                f"vendor_result: on_recognized: {message}",
                category=LOG_CATEGORY_VENDOR,
            )

            message_type = message_data.get("type")
            if message_type == "Results":
                await self.callback.on_result(message_data)

        except Exception as e:
            error_msg = f"Error processing message: {e}"
            self.ten_env.log_info(
                f"DeepgramASRRecognition._handle_message error: {error_msg}"
            )
            await self.callback.on_error(error_msg)

    async def _message_handler(self):
        """Handle incoming WebSocket messages"""
        try:
            if self.websocket is None:
                return
            ws = self.websocket
            async for message in ws:
                await self._handle_message(message)
        except websockets.exceptions.ConnectionClosed:
            self.ten_env.log_info("WebSocket connection closed")
        except Exception as e:
            error_msg = f"WebSocket message handler error: {e}"
            self.ten_env.log_info(f"### {error_msg} ###")
            await self.callback.on_error(error_msg)
        finally:
            self.is_started = False
            await self.callback.on_close()

    # This function appends query parameters to a URL
    def append_query_params(self, url: str):
        """
        Appends query parameters to a URL
        """
        base_url = self.url
        query_params = []

        excluded_params = {
            "url",
            "key",
            "api_key",
            "mute_pkg_duration_ms",
            "finalize_mode",
        }

        for param_key, value in self.config.items():
            if param_key not in excluded_params and value is not None:
                if isinstance(value, bool):
                    query_params.append(f"{param_key}={str(value).lower()}")
                elif value:
                    query_params.append(f"{param_key}={value}")

        if query_params:
            url = f"{base_url}?{'&'.join(query_params)}"
        else:
            url = base_url

        self.ten_env.log_info(f"Deepgram url: {url}")
        return url

    async def start(self, timeout=10):
        """
        Start speech recognition service
        :param timeout: Connection timeout in seconds, default 10 seconds
        """
        if self.is_connected():
            self.ten_env.log_info("Recognition already started")
            return True

        try:
            url = self.append_query_params(self.url)
            self.ten_env.log_info(f"Connecting to Deepgram: {url}")

            self.websocket = await websockets.connect(
                url,
                additional_headers=[
                    ("Authorization", f"Token {self.api_key}"),
                    ("Content-Type", "application/octet-stream"),
                ],
                open_timeout=timeout,
            )

            self.ten_env.log_info("### WebSocket opened ###")
            self.is_started = True

            # Start message handler task
            self._message_task = asyncio.create_task(self._message_handler())

            return True
        except asyncio.TimeoutError:
            error_msg = f"Connection timeout after {timeout} seconds"
            self.ten_env.log_error(f"Failed to start recognition: {error_msg}")
            await self.callback.on_error(error_msg, TIMEOUT_CODE)
            return False
        except Exception as e:
            error_msg = f"Failed to start recognition: {e}"
            self.ten_env.log_error(error_msg)
            await self.callback.on_error(error_msg)
            return False

    async def send_audio_frame(self, audio_data):
        """
        Send audio bytes directly through websocket.
        :param audio_data: Audio data (bytes)
        """
        try:
            if self.websocket is None or not self.is_connected():
                self.ten_env.log_warn(
                    "WebSocket not connected, cannot send audio"
                )
                return

            # Update timeline based on actual sent bytes
            sample_rate = self.config.get("sample_rate", 16000)
            duration_ms = int(len(audio_data) / (sample_rate / 1000 * 2))
            self.audio_timeline.add_user_audio(duration_ms)

            # Send audio directly
            await self.websocket.send(audio_data)
        except websockets.exceptions.ConnectionClosed:
            self.ten_env.log_info(
                "WebSocket connection closed while sending audio"
            )
            self.is_started = False
            await self.callback.on_error(
                "WebSocket connection closed while sending audio"
            )
        except Exception as e:
            self.ten_env.log_info(f"Failed to send audio frame: {e}")
            await self.callback.on_error(f"Failed to send audio frame: {e}")

    async def stop(self):
        """
        Stop speech recognition
        """
        if not self.is_connected():
            self.ten_env.log_info("Recognition not started")
            return

        try:
            # Send end identifier
            d = {"type": "Finalize"}
            ws = self.websocket
            if ws is not None:
                await ws.send(json.dumps(d))
            self.is_started = False
            if self.ten_env:
                self.ten_env.log_info(
                    f"vendor_cmd: {json.dumps(d)}",
                    category=LOG_CATEGORY_VENDOR,
                )

        except websockets.exceptions.ConnectionClosed:
            self.ten_env.log_info("WebSocket connection already closed")
        except Exception as e:
            self.ten_env.log_info(f"Failed to stop recognition: {e}")
            await self.callback.on_error(f"Failed to stop recognition: {e}")

    async def close(self):
        """Close WebSocket connection"""
        if self.websocket:
            try:
                if self.websocket.state == State.OPEN:
                    await self.websocket.close()
            except Exception as e:
                self.ten_env.log_info(f"Error closing websocket: {e}")

        if self._message_task and not self._message_task.done():
            self._message_task.cancel()
            try:
                await self._message_task
            except asyncio.CancelledError:
                pass

        self.is_started = False
        self.ten_env.log_info("WebSocket connection closed")

    def is_connected(self) -> bool:
        """Check if WebSocket connection is established"""
        if self.websocket is None:
            return False

        # Check if websocket is still open by checking the state
        try:
            # For websockets library, we can check the state attribute
            if hasattr(self.websocket, "state"):
                return self.is_started and self.websocket.state == State.OPEN
            # Fallback: just check if websocket exists and is_started is True
            else:
                return self.is_started
        except Exception:
            # If any error occurs, assume disconnected
            return False
