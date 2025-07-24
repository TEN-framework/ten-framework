import asyncio
import copy
import json
import ssl
import time
from typing import AsyncIterator

import websockets
from ten_runtime.async_ten_env import AsyncTenEnv

from .config import MinimaxTTS2Config


class MinimaxTTS2:
    def __init__(
        self,
        config: MinimaxTTS2Config,
        ten_env: AsyncTenEnv | None = None,
        vendor: str = "minimax"
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor

        self.stopping: bool = False
        self.ws: websockets.ClientConnection | None = None
        self.session_id: str = ""
        self.session_trace_id: str = ""

    async def start(self):
        """Preheating: establish websocket connection during initialization"""
        try:
            await self._connect()
            if self.ten_env:
                self.ten_env.log_info("MinimaxTTS2 websocket preheated successfully")
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to preheat websocket connection: {e}")
            # Don't raise here, let it retry during actual TTS requests

    async def stop(self):
        """Stop and cleanup websocket connection"""
        await self.close()

    async def get(self, text: str) -> AsyncIterator[bytes]:
        """Generate TTS audio for the given text"""
        if not text or text.strip() == "":
            return

        try:
            # Ensure websocket connection
            if not await self._ensure_connection():
                return

            # Send TTS request and yield audio chunks
            async for audio_chunk in self._process_single_tts(text):
                yield audio_chunk

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in TTS get(): {e}")
            raise

    async def _ensure_connection(self) -> bool:
        """Ensure websocket connection is established"""
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
            raise RuntimeError(f"Websocket connection failed: {error_msg}")

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
            raise RuntimeError(f"Task start failed: {error_msg}")

        if self.ten_env:
            self.ten_env.log_debug(f"websocket session ready: {self.session_id}")

    def _create_start_task_msg(self) -> dict:
        """Create task start message"""
        start_msg = copy.deepcopy(self.config.params)
        start_msg["event"] = "task_start"
        return start_msg

    async def _process_single_tts(self, text: str) -> AsyncIterator[bytes]:
        """Process a single TTS request in serial manner (like minimax copy.py)"""
        if not self.ws:
            return

        time_before_send = time.time()
        ws_req = {"event": "task_continue", "text": text}

        if self.ten_env:
            self.ten_env.log_debug(f"websocket sending task_continue: {ws_req}")

        await self.ws.send(json.dumps(ws_req))

        chunk_counter = 0
        ttfb_logged = False

        # Receive messages until is_final/task_finished/task_failed
        while True:
            try:
                tts_response_bytes = await self.ws.recv()
                tts_response = json.loads(tts_response_bytes)

                # Log response without data field
                tts_response_for_print = tts_response.copy()
                tts_response_for_print.pop("data", None)
                if self.ten_env:
                    self.ten_env.log_debug(f"recv from websocket: {tts_response_for_print}")

                tts_response_event = tts_response.get("event")
                if tts_response_event == "task_failed":
                    error_msg = tts_response.get("base_resp", {}).get("status_msg", "unknown error")
                    error_code = tts_response.get("base_resp", {}).get("status_code", 0)
                    if self.ten_env:
                        self.ten_env.log_error(f"TTS task failed: {error_msg}")
                        self.ten_env.log_error(f"TTS task failed: {error_code}")
                    # close websocket
                    await self.close()
                    break
                elif tts_response_event == "task_finished":
                    if self.ten_env:
                        self.ten_env.log_debug("tts gracefully finished")
                    break

                if tts_response.get("is_final", False):
                    if self.ten_env:
                        self.ten_env.log_debug("tts is_final received")
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

                    if not ttfb_logged:
                        ttfb = int((time.time() - time_before_send) * 1000)
                        ttfb_logged = True
                        if self.ten_env:
                            self.ten_env.log_info(
                                f"KEYPOINT [session_id:{self.session_id}] [session_trace_id:{self.session_trace_id}] "
                                f"[ttfb:{ttfb}ms] [text:{text}]"
                            )

                    chunk_counter += 1
                    if len(audio_bytes) > 0:
                        yield audio_bytes
                else:
                    if self.ten_env:
                        self.ten_env.log_warn(f"tts response no audio data, full response: {tts_response}")
                    break  # No more audio data, end this request

            except websockets.exceptions.ConnectionClosed:
                if self.ten_env:
                    self.ten_env.log_warn("Websocket connection closed during TTS processing")
                self.ws = None
                break
            except websockets.exceptions.ConnectionClosedOK:
                if self.ten_env:
                    self.ten_env.log_warn("Websocket connection closed OK during TTS processing")
                self.ws = None
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Error processing TTS response: {e}")
                break

    async def close(self):
        """Close the websocket connection"""
        self.stopping = True
        if self.ws:
            await self.ws.close()
            self.ws = None