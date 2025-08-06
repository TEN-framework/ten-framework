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
from typing import AsyncIterator
from dataclasses import dataclass

from ten_runtime import AsyncTenEnv
from .config import MinimaxTTSWebsocketConfig

# TTS Events
EVENT_TTSSentenceStart = 350
EVENT_TTSSentenceEnd = 351
EVENT_TTSResponse = 352
EVENT_TTSTaskFinished = 353
EVENT_TTSFlush = 354


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
        vendor: str = "minimax"
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor

        self.stopping: bool = False
        self._is_cancelled: bool = False
        self._connection_is_dirty: bool = False
        self.ws: websockets.ClientConnection | None = None
        self.session_id: str = ""
        self.session_trace_id: str = ""

    async def start(self):
        """Preheating: establish websocket connection during initialization"""
        try:
            await self._connect()
            if self.ten_env:
                self.ten_env.log_info("MinimaxTTSWebsocket websocket preheated successfully")
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to preheat websocket connection: {e}")
            # Don't raise here, let it retry during actual TTS requests

    async def stop(self):
        """Stop and cleanup websocket connection"""
        await self.close()

    async def cancel(self):
        """
        Cancel the current TTS task by closing the websocket connection.
        This will trigger a ConnectionClosed exception in the processing loop.
        """
        if self.ten_env:
            self.ten_env.log_debug("Cancelling current TTS task by closing websocket.")
        self._is_cancelled = True
        if self.ws:
            await self.ws.close()

    async def get(self, text: str) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Generate TTS audio for the given text, returns (audio_data, event_status)"""
        if not text or text.strip() == "":
            return

        self._is_cancelled = False
        try:
            # Ensure websocket connection
            if not await self._ensure_connection():
                return

            # Send TTS request and yield audio chunks with event status
            async for audio_chunk, event_status in self._process_single_tts(text):
                yield audio_chunk, event_status

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in TTS get(): {e}")
            raise

    async def _ensure_connection(self) -> bool:
        """Ensure websocket connection is established"""
        # If connection is marked as dirty from a previous cancellation, recycle it.
        if self._connection_is_dirty:
            if self.ten_env:
                self.ten_env.log_info("Connection is dirty, recycling.")
            await self.close()
            self._connection_is_dirty = False

        # If no connection or connection seems closed, reconnect
        if not self.ws:
            try:
                await self._connect()
                return True
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Failed to establish websocket connection: {e}")
                return False
        return True

    async def _connect(self) -> None:
        """Establish websocket connection and initialize session"""
        headers = {"Authorization": f"Bearer {self.config.api_key}"}

        if self.ten_env:
            self.ten_env.log_info(f"websocket connecting to {self.config.to_str()}")

        ssl_context = ssl.create_default_context()
        ssl_context.check_hostname = False
        ssl_context.verify_mode = ssl.CERT_NONE

        session_start_time = time.time()

        # Connect to websocket
        self.ws = await websockets.connect(
            self.config.url,
            additional_headers=headers,
            ssl=ssl_context,
            max_size=1024 * 1024 * 16,
        )

        # Get trace info
        self.session_trace_id = self.ws.response.headers.get("Trace-Id", "")
        session_alb_request_id = self.ws.response.headers.get("alb_request_id", "")

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
            self.ten_env.log_info(f"websocket init response: {init_response}")

        if init_response.get("event") != "connected_success":
            error_msg = init_response.get("base_resp", {}).get("status_msg", "unknown error")
            error_code = init_response.get("base_resp", {}).get("status_code", 0)
            self.ten_env.log_error(f"Websocket connection failed: {error_msg}")
            self.ten_env.log_error(f"Websocket connection failed: {error_code}")
            raise MinimaxTTSTaskFailedException(error_msg, error_code)

        self.session_id = init_response.get("session_id", "")
        if self.ten_env:
            self.ten_env.log_debug(f"websocket connected success, session_id: {self.session_id}")

        # Start task
        start_task_msg = self._create_start_task_msg()
        if self.ten_env:
            self.ten_env.log_debug(f"sending task_start: {start_task_msg}")

        await self.ws.send(json.dumps(start_task_msg))
        start_task_response_bytes = await self.ws.recv()
        start_task_response = json.loads(start_task_response_bytes)

        if self.ten_env:
            self.ten_env.log_debug(f"start task response: {start_task_response}")

        if start_task_response.get("event") != "task_started":
            error_msg = start_task_response.get("base_resp", {}).get("status_msg", "unknown error")
            error_code = start_task_response.get("base_resp", {}).get("status_code", 0)
            self.ten_env.log_error(f"Task start failed: {error_msg}")
            self.ten_env.log_error(f"Task start failed: {error_code}")
            raise MinimaxTTSTaskFailedException(error_msg, error_code)

        if self.ten_env:
            self.ten_env.log_debug(f"websocket session ready: {self.session_id}")

    def _create_start_task_msg(self) -> dict:
        """Create task start message"""
        start_msg = copy.deepcopy(self.config.params)
        start_msg["event"] = "task_start"
        return start_msg

    async def _process_single_tts(self, text: str) -> AsyncIterator[tuple[bytes | None, int | None]]:
        """Process a single TTS request in serial manner (like minimax copy.py)"""
        if not self.ws:
            return

        time_before_send = time.time()
        ws_req = {"event": "task_continue", "text": text}

        if self.ten_env:
            self.ten_env.log_debug(f"websocket sending task_continue: {ws_req}")

        await self.ws.send(json.dumps(ws_req))

        chunk_counter = 0

        # Receive messages until is_final/task_finished/task_failed
        while True:
            try:
                tts_response_bytes = await self.ws.recv()
                tts_response = json.loads(tts_response_bytes)

                # Log response without data field
                tts_response_for_print = tts_response.copy()
                tts_response_for_print.pop("data", None)
                if self.ten_env:
                    self.ten_env.log_info(f"recv from websocket: {tts_response_for_print}")

                tts_response_event = tts_response.get("event")
                if tts_response_event == "task_failed":
                    error_msg = tts_response.get("base_resp", {}).get("status_msg", "unknown error")
                    error_code = tts_response.get("base_resp", {}).get("status_code", 0)
                    if self.ten_env:
                        self.ten_env.log_error(f"TTS task failed: {error_msg}")
                        self.ten_env.log_error(f"TTS task failed: {error_code}")
                    # close websocket
                    await self.close()
                    # Raise exception to let extension.py handle the error
                    raise MinimaxTTSTaskFailedException(error_msg, error_code)
                elif tts_response_event == "task_finished":
                    if self.ten_env:
                        self.ten_env.log_debug("tts gracefully finished")
                    # Return event status for task finished
                    yield None, EVENT_TTSTaskFinished
                    break

                if tts_response.get("is_final", False):
                    if self.ten_env:
                        self.ten_env.log_debug("tts is_final received")
                    # Return event status for is_final
                    yield None, EVENT_TTSSentenceEnd
                    break

                # Process audio data
                if "data" in tts_response and "audio" in tts_response["data"]:
                    audio = tts_response["data"]["audio"]
                    without_audio = tts_response["data"].copy()
                    without_audio.pop("audio", None)

                    if self.ten_env:
                        self.ten_env.log_debug(f"audio chunk #{chunk_counter}, without_audio: {without_audio}")

                    audio_bytes = bytes.fromhex(audio)

                    if self.ten_env:
                        self.ten_env.log_debug(
                            f"audio chunk #{chunk_counter}, hex bytes: {len(audio)}, audio bytes: {len(audio_bytes)}"
                        )

                    chunk_counter += 1
                    if len(audio_bytes) > 0:
                        yield audio_bytes, EVENT_TTSResponse
                else:
                    if self.ten_env:
                        self.ten_env.log_warn(f"tts response no audio data, full response: {tts_response}")
                    break  # No more audio data, end this request

                # Check for cancellation signal
                if self._is_cancelled:
                    if self.ten_env:
                        self.ten_env.log_info("Cancellation flag detected, stopping TTS processing.")
                    break

            except websockets.exceptions.ConnectionClosed:
                if self.ten_env:
                    if self._is_cancelled:
                        self.ten_env.log_info("Websocket connection closed due to cancellation.")
                        yield None, EVENT_TTSFlush
                    else:
                        self.ten_env.log_warn("Websocket connection closed during TTS processing")
                self.ws = None
                break
            except websockets.exceptions.ConnectionClosedOK:
                if self.ten_env:
                    if self._is_cancelled:
                        self.ten_env.log_warn("Websocket connection closed OK during TTS processing (cancelled)")
                    else:
                        self.ten_env.log_warn("Websocket connection closed OK during TTS processing")
                self.ws = None
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Error processing TTS response: {e}")
                raise

    async def close(self):
        """Close the websocket connection"""
        self.stopping = True
        if self.ws:
            await self.ws.close()
            self.ws = None