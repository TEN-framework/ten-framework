#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import copy
import json
import ssl
import time
import websockets
from typing import AsyncIterator, Union
from datetime import datetime

from ten_runtime import AsyncTenEnv
from .config import MinimaxTTSWebsocketConfig

# TTS Events
EVENT_TTSSentenceStart = 350
EVENT_TTSSentenceEnd = 351
EVENT_TTSResponse = 352
EVENT_TTSTaskFinished = 353
EVENT_TTSFlush = 354
EVENT_TTS_TTFB_METRIC = 355


class MinimaxTTSTaskFailedException(Exception):
    """Exception raised when Minimax TTS task fails"""

    def __init__(self, error_msg: str, error_code: int):
        self.error_msg = error_msg
        self.error_code = error_code
        super().__init__(f"TTS task failed: {error_msg} (code: {error_code})")


class MinimaxTTSWebsocket:
    def __init__(
        self,
        config: MinimaxTTSWebsocketConfig,
        ten_env: AsyncTenEnv | None = None,
        vendor: str = "minimax",
        error_callback=None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.error_callback = (
            error_callback  # Callback for sending errors to extension
        )

        self.stopping: bool = False
        self.discarding: bool = False
        self.first_chunk_processed: bool = False
        self.ws: websockets.ClientConnection | None = None
        self.session_id: str = ""
        self.session_trace_id: str = ""

        # WebSocket resource management
        self.ws_released_event: asyncio.Event = asyncio.Event()
        self.ws_released_event.set()  # Initially set since no WS is active
        self.stopped_event: asyncio.Event = asyncio.Event()

    async def start(self):
        """Start the WebSocket processor task"""
        if self.ten_env:
            self.ten_env.log_info("Starting MinimaxTTSWebsocket processor")
        asyncio.create_task(self._process_websocket())

    async def stop(self):
        """Stop and cleanup websocket connection"""
        self.stopping = True
        self.first_chunk_processed = False
        await self.cancel()
        # Wait for processor to exit
        await self.stopped_event.wait()

    async def cancel(self):
        """Cancel current operations and wait for resource release"""
        if self.ten_env:
            self.ten_env.log_info("Cancelling TTS operations")

        if self.discarding:
            return  # Already cancelling

        self.discarding = True
        self.first_chunk_processed = False

        # Wait for WS resource to be released
        await self.ws_released_event.wait()

    async def get(
        self, text: str
    ) -> AsyncIterator[tuple[Union[bytes, int, None], int | None]]:
        """Generate TTS audio for the given text, returns (data, event_status)"""
        if not text or text.strip() == "":
            return

        self.discarding = False

        try:
            if self.ten_env:
                self.ten_env.log_info(f"get TTS for text: {text}")

            # Wait for WebSocket to be available (串行访问)
            await self.ws_released_event.wait()
            if self.ten_env:
                self.ten_env.log_info("ws_released_event cleared")

            # Ensure we have a valid WebSocket connection
            if not self.ws:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "No WebSocket connection available for TTS"
                    )
                return

            # Process TTS request directly
            async for audio_chunk, event_status in self._process_single_tts(
                text
            ):
                if self.discarding:
                    break
                yield audio_chunk, event_status

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in TTS get(): {e}")
            raise

    async def _process_single_tts(
        self, text: str
    ) -> AsyncIterator[tuple[Union[bytes, int, None], int | None]]:
        """Process a single TTS request"""
        if not self.ws:
            return

        ws_req = {"event": "task_continue", "text": text}

        if self.ten_env:
            self.ten_env.log_debug(f"websocket sending task_continue: {ws_req}")

        try:
            send_time = datetime.now()
            await self.ws.send(json.dumps(ws_req))
        except (
            websockets.exceptions.ConnectionClosed,
            websockets.exceptions.ConnectionClosedOK,
        ) as e:
            if self.ten_env:
                self.ten_env.log_warn(f"Connection closed during send: {e}")
            return

        chunk_counter = 0

        # Receive responses until is_final/task_finished/task_failed
        while not self.stopping and not self.discarding:
            if not self.ws:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "WebSocket connection lost during processing"
                    )
                break

            try:
                tts_response_bytes = await self.ws.recv()
                tts_response = json.loads(tts_response_bytes)

                # Log response without data field
                # tts_response_for_print = tts_response.copy()
                # tts_response_for_print.pop("data", None)
                # if self.ten_env:
                #     self.ten_env.log_debug(
                #         f"recv from websocket: {tts_response_for_print}"
                #     )

                tts_response_event = tts_response.get("event")
                if tts_response_event == "task_failed":
                    error_msg = tts_response.get("base_resp", {}).get(
                        "status_msg", "unknown error"
                    )
                    error_code = tts_response.get("base_resp", {}).get(
                        "status_code", 0
                    )
                    if self.ten_env:
                        self.ten_env.log_error(f"TTS task failed: {error_msg}")
                    raise MinimaxTTSTaskFailedException(error_msg, error_code)
                elif tts_response_event == "task_finished":
                    if self.ten_env:
                        self.ten_env.log_debug("tts gracefully finished")
                    yield None, EVENT_TTSTaskFinished
                    break

                if tts_response.get("is_final", False):
                    if self.ten_env:
                        self.ten_env.log_debug("tts is_final received")
                    yield None, EVENT_TTSSentenceEnd
                    break

                # Process audio data
                if "data" in tts_response and "audio" in tts_response["data"]:
                    audio = tts_response["data"]["audio"]
                    audio_bytes = bytes.fromhex(audio)

                    # if self.ten_env:
                    #     self.ten_env.log_info(
                    #         f"audio chunk #{chunk_counter}, hex bytes: {len(audio)}, audio bytes: {len(audio_bytes)}"
                    #     )

                    if not self.first_chunk_processed:
                        first_byte_time = datetime.now()
                        ttfb_ms = int(
                            (first_byte_time - send_time).total_seconds() * 1000
                        )
                        yield ttfb_ms, EVENT_TTS_TTFB_METRIC
                        self.first_chunk_processed = True

                    chunk_counter += 1
                    if len(audio_bytes) > 0:
                        yield audio_bytes, EVENT_TTSResponse
                else:
                    if self.ten_env:
                        self.ten_env.log_warn(
                            f"tts response no audio data: {tts_response}"
                        )
                    break

            except websockets.exceptions.ConnectionClosedOK:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "Websocket connection closed OK during TTS processing"
                    )
                break
            except websockets.exceptions.ConnectionClosed:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "Websocket connection closed during TTS processing"
                    )
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(
                        f"Error processing TTS response: {e}"
                    )
                self.ws = None
                raise

    async def _process_websocket(self) -> None:
        """Main WebSocket connection management loop"""
        if self.ten_env:
            self.ten_env.log_debug("WebSocket processor started")

        while not self.stopping:
            # Clear the event at the start of each connection attempt
            self.ws_released_event.clear()
            if self.ten_env:
                self.ten_env.log_debug(
                    "Starting WebSocket connection attempt..."
                )

            session_alb_request_id = ""
            session_id = ""

            try:
                # Establish connection
                headers = {"Authorization": f"Bearer {self.config.api_key}"}
                ssl_context = ssl.create_default_context()
                ssl_context.check_hostname = False
                ssl_context.verify_mode = ssl.CERT_NONE

                session_start_time = time.time()
                if self.ten_env:
                    self.ten_env.log_debug(
                        f"websocket connecting to {self.config.to_str()}"
                    )

                self.ws = await websockets.connect(
                    self.config.url,
                    additional_headers=headers,
                    ssl=ssl_context,
                    max_size=1024 * 1024 * 16,
                )

                # Get trace info
                try:
                    self.session_trace_id = self.ws.response.headers.get(
                        "Trace-Id", ""
                    )
                    session_alb_request_id = self.ws.response.headers.get(
                        "alb_request_id", ""
                    )
                except Exception:
                    pass

                elapsed = int((time.time() - session_start_time) * 1000)
                if self.ten_env:
                    self.ten_env.log_info(
                        f"websocket connected, session_trace_id: {self.session_trace_id}, "
                        f"session_alb_request_id: {session_alb_request_id}, cost_time {elapsed}ms"
                    )

                # Handle init response
                init_response_bytes = await self.ws.recv()
                init_response = json.loads(init_response_bytes)
                if self.ten_env:
                    self.ten_env.log_debug(
                        f"websocket init response: {init_response}"
                    )

                if init_response.get("event") != "connected_success":
                    error_msg = init_response.get("base_resp", {}).get(
                        "status_msg", "unknown error"
                    )
                    error_code = init_response.get("base_resp", {}).get(
                        "status_code", 0
                    )
                    if self.ten_env:
                        self.ten_env.log_error(
                            f"Websocket connection failed: {error_msg}, "
                            f"error_code: {error_code}"
                        )
                    continue

                self.session_id = init_response.get("session_id", "")
                session_id = self.session_id

                # Start task
                start_task_msg = self._create_start_task_msg()
                if self.ten_env:
                    self.ten_env.log_debug(
                        f"sending task_start: {start_task_msg}"
                    )

                await self.ws.send(json.dumps(start_task_msg))
                start_task_response_bytes = await self.ws.recv()
                start_task_response = json.loads(start_task_response_bytes)

                if self.ten_env:
                    self.ten_env.log_debug(
                        f"start task response: {start_task_response}"
                    )

                if start_task_response.get("event") != "task_started":
                    error_msg = start_task_response.get("base_resp", {}).get(
                        "status_msg", "unknown error"
                    )
                    error_code = start_task_response.get("base_resp", {}).get(
                        "status_code", 0
                    )
                    if self.ten_env:
                        self.ten_env.log_error(
                            f"Task start failed: {error_msg}"
                        )
                    continue

                if self.ten_env:
                    self.ten_env.log_debug(
                        f"websocket session ready: {session_id}"
                    )

                # WebSocket is now ready, signal that it's available for use
                self.ws_released_event.set()
                if self.ten_env:
                    self.ten_env.log_debug(
                        "ws_released_event set - WebSocket ready for use"
                    )

                # Connection established successfully, keep it alive
                # Wait for the connection to be closed or stop signal
                while not self.stopping and not self.discarding and self.ws:
                    await asyncio.sleep(
                        0.1
                    )  # Faster response to discarding signal

            except websockets.exceptions.ConnectionClosedError as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket ConnectionClosedError: {e}"
                    )
                await self._send_websocket_error(
                    "WebSocket connection closed unexpectedly", str(e)
                )
            except websockets.exceptions.ConnectionClosedOK as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket ConnectionClosedOK: {e}"
                    )
                # ConnectionClosedOK is normal, don't send error
            except websockets.exceptions.InvalidHandshake as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket InvalidHandshake: {e}"
                    )
                # Check if it's a fatal HTTP 200 rejection
                await self._send_websocket_error(
                    "WebSocket handshake failed", str(e), is_fatal=True
                )
                await asyncio.sleep(1)  # Wait before reconnect
            except websockets.exceptions.WebSocketException as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket exception: {e}"
                    )
                await self._send_websocket_error(
                    "WebSocket protocol error", str(e)
                )
                await asyncio.sleep(1)  # Wait before reconnect
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, unexpected exception: {e}"
                    )
                await self._send_websocket_error(
                    "Unexpected WebSocket error", str(e)
                )
                await asyncio.sleep(1)  # Wait before reconnect
            finally:
                self.ws = None
                self.discarding = False
                self.ws_released_event.set()
                if self.ten_env:
                    self.ten_env.log_debug(
                        f"session_id: {session_id}, WebSocket processor cycle finished"
                    )

        self.stopped_event.set()
        if self.ten_env:
            self.ten_env.log_debug("WebSocket processor exited")

    async def _send_websocket_error(
        self, message: str, detail: str, is_fatal: bool = False
    ) -> None:
        """Send WebSocket error through callback to extension"""
        if self.error_callback:
            try:
                await self.error_callback(message, detail, is_fatal)
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Error calling error callback: {e}")

    def _create_start_task_msg(self) -> dict:
        """Create task start message"""
        start_msg = copy.deepcopy(self.config.params)
        start_msg["event"] = "task_start"
        return start_msg

    async def close(self):
        """Close the websocket connection"""
        self.stopping = True
        if self.ws:
            try:
                await self.ws.close()
            except Exception:
                pass  # Ignore close errors
            self.ws = None
