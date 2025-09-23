#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import ssl
import time
import uuid
import websockets
from typing import Callable, Optional

from ten_runtime import AsyncTenEnv
from .config import AliyunTTSPythonConfig
from ten_ai_base.struct import TTSTextInput, TTSTextResult, TTSWord

# TTS Events
EVENT_TTSSentenceStart = 350
EVENT_TTSSentenceEnd = 351
EVENT_TTSResponse = 352
EVENT_TTSTaskFinished = 353
EVENT_TTSFlush = 354

from ten_ai_base.const import LOG_CATEGORY_VENDOR, LOG_CATEGORY_KEY_POINT


class AliyunTTSTaskFailedException(Exception):
    """Exception raised when Aliyun TTS task fails"""

    def __init__(self, error_msg: str, error_code: int):
        self.error_msg = error_msg
        self.error_code = error_code
        super().__init__(f"TTS task failed: {error_msg} (code: {error_code})")


class _AliyunTTSInstance:
    """Handles a single, stateful WebSocket connection instance."""

    def __init__(
        self,
        config: AliyunTTSPythonConfig,
        ten_env: AsyncTenEnv | None = None,
        vendor: str = "aliyun",
        on_transcription: Optional[
            Callable[[TTSTextResult], asyncio.Future]
        ] = None,
        on_error: Optional[
            Callable[[AliyunTTSTaskFailedException], None]
        ] = None,
        on_audio_data: Optional[
            Callable[[bytes, int, int], asyncio.Future]
        ] = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor

        self.stopping: bool = False
        self.discarding: bool = False
        self.ws: websockets.ClientConnection | None = None
        self.session_id: str = ""
        self.session_trace_id: str = ""
        self.task_id: str = ""
        self.tts_task_queue: asyncio.Queue = asyncio.Queue()
        self.on_transcription = on_transcription
        self.on_error = on_error
        self.on_audio_data = on_audio_data
        self.receive_task: asyncio.Task | None = None

        # Track current request for transcription
        self.base_request_start_ms: int = 0
        self.current_request_start_ms: int = 0  # 记录当前request的开始时间
        self.estimated_duration_this_request: int = (
            0  # 当前request的估算时长累积
        )
        self.audio_sample_rate: int = (
            16000  # 音频采样率，从extra_info中获取，默认16000
        )
        self.audio_channel: int = (
            1  # 音频声道数，从extra_info中获取，默认1（单声道）
        )
        self.request_id = -1
        self.last_word_end_ms: int = 0  # 记录上一个已处理单词的结束时间

        # Simple synchronization
        self.stopped_event: asyncio.Event = asyncio.Event()

    async def start(self):
        """Start the WebSocket processor task"""
        if self.ten_env:
            self.ten_env.log_info("Starting AliyunTTSPython processor")
        asyncio.create_task(self._process_websocket())

    async def stop(self):
        """Stop and cleanup websocket connection"""
        self.stopping = True
        if self.receive_task and not self.receive_task.done():
            self.receive_task.cancel()
        await self.cancel()
        # Wait for processor to exit
        await self.stopped_event.wait()

    async def cancel(self):
        """Cancel current operations"""
        if self.ten_env:
            self.ten_env.log_info("Cancelling TTS operations")

        if self.discarding:
            return  # Already cancelling

        self.discarding = True
        if self.receive_task and not self.receive_task.done():
            self.receive_task.cancel()

        # Clear the task queue
        while not self.tts_task_queue.empty():
            try:
                self.tts_task_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

        # Insert sentinel to wake up queue.get()
        await self.tts_task_queue.put(None)

    async def close(self):
        """Close the websocket connection"""
        self.stopping = True
        if self.ws:
            try:
                await self.ws.close()
            except Exception:
                pass  # Ignore close errors
            self.ws = None

    async def get(self, tts_input: TTSTextInput):
        """Send TTS request. Audio data will be sent via callback."""
        if self.discarding:
            if self.ten_env:
                self.ten_env.log_info(
                    "Discarding get() request because client is in cancelling state."
                )
            return

        # Simply put request in task queue - audio will be sent via callback
        await self.tts_task_queue.put(tts_input)

    def _create_start_synthesis_msg(self) -> dict:
        """Create StartSynthesis message"""
        return {
            "header": {
                "message_id": uuid.uuid4().hex,
                "task_id": self.task_id,
                "namespace": "FlowingSpeechSynthesizer",
                "name": "StartSynthesis",
                "appkey": self.config.appkey,
            },
            "payload": self.config.params,
        }

    def _create_run_synthesis_msg(self, text: str) -> dict:
        """Create RunSynthesis message"""
        return {
            "header": {
                "message_id": uuid.uuid4().hex,
                "task_id": self.task_id,
                "namespace": "FlowingSpeechSynthesizer",
                "name": "RunSynthesis",
                "appkey": self.config.appkey,
            },
            "payload": {"text": text},
        }

    def _create_stop_synthesis_msg(self) -> dict:
        """Create StopSynthesis message"""
        return {
            "header": {
                "message_id": uuid.uuid4().hex,
                "task_id": self.task_id,
                "namespace": "FlowingSpeechSynthesizer",
                "name": "StopSynthesis",
                "appkey": self.config.appkey,
            }
        }

    async def _process_websocket(self) -> None:
        """Main WebSocket connection management loop"""
        if self.ten_env:
            self.ten_env.log_info("WebSocket processor started")

        while not self.stopping:
            self.task_id = uuid.uuid4().hex
            session_id = ""
            try:
                # Establish connection
                ssl_context = ssl.create_default_context()
                ssl_context.check_hostname = False
                ssl_context.verify_mode = ssl.CERT_NONE

                connect_url = f"{self.config.url}?token={self.config.token}"
                session_start_time = time.time()
                if self.ten_env:
                    self.ten_env.log_info(
                        f"websocket connecting to {connect_url}"
                    )

                self.ws = await websockets.connect(
                    connect_url,
                    ssl=ssl_context,
                    max_size=1024 * 1024 * 16,
                )

                elapsed = int((time.time() - session_start_time) * 1000)
                if self.ten_env:
                    self.ten_env.log_info(
                        f"websocket connected, cost_time {elapsed}ms"
                    )
                    self.ten_env.log_info(
                        f"vendor_status: connected to: {self.config.url}",
                        category=LOG_CATEGORY_VENDOR,
                    )

                # Start task
                start_task_msg = self._create_start_synthesis_msg()
                if self.ten_env:
                    self.ten_env.log_info(
                        f"sending StartSynthesis: {start_task_msg}"
                    )
                await self.ws.send(json.dumps(start_task_msg))

                # Wait for SynthesisStarted event
                start_task_response_bytes = await self.ws.recv()
                if not isinstance(start_task_response_bytes, str):
                    raise AliyunTTSTaskFailedException(
                        "Expected text frame for SynthesisStarted, got binary",
                        -1,
                    )
                start_task_response = json.loads(start_task_response_bytes)

                if self.ten_env:
                    self.ten_env.log_info(
                        f"start task response: {start_task_response}"
                    )

                header = start_task_response.get("header", {})
                session_id = header.get("task_id", "")
                self.session_id = session_id
                if header.get("name") != "SynthesisStarted":
                    status = header.get("status", -1)
                    status_message = header.get(
                        "status_message", "Unknown error"
                    )
                    raise AliyunTTSTaskFailedException(status_message, status)

                if self.ten_env:
                    self.ten_env.log_info(
                        f"websocket session ready: {session_id}"
                    )

                # Process TTS tasks with concurrent send/receive
                self.receive_task = asyncio.create_task(
                    self._receive_loop(self.ws)
                )
                send_task = asyncio.create_task(self._send_loop(self.ws))
                await asyncio.gather(send_task, self.receive_task)

            except AliyunTTSTaskFailedException as e:
                if self.ten_env:
                    self.ten_env.log_error(
                        f"vendor_error: code: {e.error_code} reason: {e.error_msg}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                if self.on_error:
                    self.on_error(e)
                await asyncio.sleep(1)
            except websockets.exceptions.ConnectionClosedError as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket ConnectionClosedError: {e}"
                    )
            except websockets.exceptions.ConnectionClosedOK as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket ConnectionClosedOK: {e}"
                    )
            except websockets.exceptions.InvalidHandshake as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket InvalidHandshake: {e}"
                    )
                if self.on_error:
                    exception = AliyunTTSTaskFailedException(str(e), -1)
                    self.on_error(exception)
                await asyncio.sleep(1)  # Wait before reconnect
            except websockets.exceptions.WebSocketException as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, websocket exception: {e}"
                    )
                await asyncio.sleep(1)  # Wait before reconnect
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_warn(
                        f"session_id: {session_id}, unexpected exception: {e}"
                    )
                await asyncio.sleep(1)  # Wait before reconnect
            finally:
                self.ws = None
                self.discarding = False
                if self.ten_env:
                    self.ten_env.log_info(
                        f"session_id: {session_id}, WebSocket processor cycle finished"
                    )

        self.stopped_event.set()
        if self.ten_env:
            self.ten_env.log_info("WebSocket processor exited")

    async def _send_loop(self, ws: websockets.ClientConnection):
        """Continuously send TTS tasks from the queue without waiting for responses."""
        while not self.stopping:
            if self.discarding:
                return

            tts_input = await self.tts_task_queue.get()
            if tts_input is None:
                return  # Sentinel

            if self.request_id != tts_input.request_id:
                self.request_id = tts_input.request_id

            try:
                if tts_input.text:
                    run_msg = self._create_run_synthesis_msg(tts_input.text)
                    await ws.send(json.dumps(run_msg))
                    if self.ten_env:
                        self.ten_env.log_info(
                            f"sent RunSynthesis with text: {tts_input.text}"
                        )

                if tts_input.text_input_end:
                    stop_msg = self._create_stop_synthesis_msg()
                    await ws.send(json.dumps(stop_msg))
                    if self.ten_env:
                        self.ten_env.log_info("sent StopSynthesis")
            except websockets.exceptions.ConnectionClosed:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "Connection closed during send, putting task back."
                    )
                await self.tts_task_queue.put(
                    tts_input
                )  # Put it back for next connection
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Error in send loop: {e}")
                break

    async def _receive_loop(self, ws: websockets.ClientConnection):
        """Continuously receive messages from websocket and handle them."""
        while not self.stopping and not self.discarding:
            try:
                message = await ws.recv()
                if isinstance(message, bytes):
                    await self._handle_audio_data(message)
                elif isinstance(message, str):
                    await self._handle_event_data(json.loads(message))
            except asyncio.CancelledError:
                break
            except websockets.exceptions.ConnectionClosedOK as e:
                if self.ten_env:
                    self.ten_env.log_info(f"Connection closed OK: {e}")
                break
            except websockets.exceptions.ConnectionClosed:
                if self.ten_env:
                    self.ten_env.log_warn(
                        "Connection closed during receive loop."
                    )
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Error in receive loop: {e}")
                if self.on_error:
                    self.on_error(AliyunTTSTaskFailedException(str(e), -1))
                break

    async def _handle_audio_data(self, audio_bytes: bytes):
        if not self.on_audio_data or len(audio_bytes) == 0:
            return

        # Estimate timestamp
        if self.current_request_start_ms == 0:
            self.base_request_start_ms = int(time.time() * 1000)
            self.current_request_start_ms = self.base_request_start_ms
        audio_timestamp = (
            self.current_request_start_ms + self.estimated_duration_this_request
        )

        bytes_per_sample = 2  # 16bit
        estimated_chunk_duration = (
            len(audio_bytes)
            * 1000
            // (self.audio_sample_rate * bytes_per_sample * self.audio_channel)
        )
        self.estimated_duration_this_request += estimated_chunk_duration

        try:
            await self.on_audio_data(
                audio_bytes, EVENT_TTSResponse, audio_timestamp
            )
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in audio data callback: {e}")

    async def _handle_event_data(self, event: dict):
        header = event.get("header", {})
        event_name = header.get("name")
        status = header.get("status")

        if status != 20000000:
            error_msg = header.get(
                "status_message", "Unknown error from Aliyun"
            )
            if self.ten_env:
                self.ten_env.log_error(
                    f"Aliyun TTS error: {status} - {error_msg}"
                )
            if self.on_error:
                self.on_error(AliyunTTSTaskFailedException(error_msg, status))
            return

        if event_name in ("SentenceSynthesis", "SentenceEnd"):
            if self.on_audio_data and event_name == "SentenceEnd":
                await self.on_audio_data(b"", EVENT_TTSSentenceEnd, 0)

            if self.config.enable_words and self.on_transcription:
                payload = event.get("payload", {})
                if "subtitles" in payload:
                    words = self._process_aliyun_subtitles(payload["subtitles"])
                    if words:
                        text = "".join(w.word for w in words)
                        transcription = self._create_tts_text_result(
                            text=text,
                            words=words,
                            request_id=str(self.request_id),
                            text_result_end=(event_name == "SentenceEnd"),
                        )
                        await self._send_transcription_if_enabled(transcription)

        elif event_name == "SynthesisCompleted":
            if self.ten_env:
                self.ten_env.log_info("SynthesisCompleted received.")
            if self.on_audio_data:
                await self.on_audio_data(b"", EVENT_TTSTaskFinished, 0)

        elif event_name == "SentenceBegin":
            if self.on_audio_data:
                await self.on_audio_data(b"", EVENT_TTSSentenceStart, 0)

    def _process_aliyun_subtitles(self, subtitles_data: list) -> list[TTSWord]:
        words = []
        for item in subtitles_data:
            text = item.get("text")
            if not text:
                continue

            # Aliyun provides character-level timestamps
            word = TTSWord(
                word=text,
                start_ms=item.get("begin_time", 0),
                duration_ms=item.get("end_time", 0) - item.get("begin_time", 0),
            )
            words.append(word)
        return words

    def _create_tts_text_result(
        self,
        text: str,
        words: list[TTSWord],
        request_id: str = "",
        start_ms: int = 0,
        duration_ms: int = 0,
        metadata: dict | None = None,
        text_result_end: bool = False,
    ) -> TTSTextResult:
        """Create TTSTextResult object"""
        actual_start_ms = (
            words[0].start_ms
            if words
            else (start_ms or self.current_request_start_ms)
        )
        if self.ten_env:
            self.ten_env.log_info(
                f"create_tts_text_result text={text}, start_ms={actual_start_ms}"
            )

        return TTSTextResult(
            request_id=request_id,
            text=text,
            start_ms=actual_start_ms,
            duration_ms=duration_ms,
            words=words or [],
            text_result_end=text_result_end,
            metadata=metadata or {},
        )

    async def _send_transcription_if_enabled(
        self, transcription: TTSTextResult
    ) -> None:
        """Send transcription data if enabled"""
        if self.config.enable_words and self.on_transcription:
            try:
                await self.on_transcription(transcription)
                if self.ten_env:
                    self.ten_env.log_info(
                        f"send tts_text_result: {transcription} of request id: {transcription.request_id}",
                        category=LOG_CATEGORY_KEY_POINT,
                    )
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Failed to send transcription: {e}")


class AliyunTTSPython:
    """
    Manages Aliyun TTS client instances, providing a stable interface
    that handles non-blocking cancels and reconnections via instance swapping.
    """

    def __init__(
        self,
        config: AliyunTTSPythonConfig,
        ten_env: AsyncTenEnv | None = None,
        vendor: str = "aliyun",
        on_transcription: Optional[
            Callable[[TTSTextResult], asyncio.Future]
        ] = None,
        on_error: Optional[
            Callable[[AliyunTTSTaskFailedException], None]
        ] = None,
        on_audio_data: Optional[
            Callable[[bytes, int, int], asyncio.Future]
        ] = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.on_transcription = on_transcription
        self.on_error = on_error
        self.on_audio_data = on_audio_data
        self.current_client: _AliyunTTSInstance = self._create_new_client()
        self.old_clients: list[_AliyunTTSInstance] = []
        self.cleanup_task: asyncio.Task | None = None

    def _create_new_client(self) -> "_AliyunTTSInstance":
        return _AliyunTTSInstance(
            config=self.config,
            ten_env=self.ten_env,
            vendor=self.vendor,
            on_transcription=self.on_transcription,
            on_error=self.on_error,
            on_audio_data=self.on_audio_data,
        )

    async def start(self):
        """Start the WebSocket processor and the cleanup task."""
        if self.ten_env:
            self.ten_env.log_info("Starting AliyunTTSPython Manager")
        asyncio.create_task(self.current_client.start())
        self.cleanup_task = asyncio.create_task(self._cleanup_old_clients())

    async def stop(self):
        """Stop the current client and all old clients."""
        if self.cleanup_task:
            self.cleanup_task.cancel()

        tasks = [self.current_client.stop()]
        for client in self.old_clients:
            tasks.append(client.stop())
        await asyncio.gather(*tasks)

    async def cancel(self):
        """
        Perform a non-blocking cancel by swapping the client instance.
        The old client is stopped in the background.
        """
        if self.ten_env:
            self.ten_env.log_info(
                "Manager received cancel request, swapping instance."
            )

        if self.current_client:
            old_client = self.current_client
            # Immediately create and start a new client BEFORE cancelling the old one
            # This prevents new requests from being routed to the cancelled client
            self.current_client = self._create_new_client()
            asyncio.create_task(self.current_client.start())

            # Now cancel and cleanup the old client
            self.old_clients.append(old_client)
            await old_client.cancel()  # Use await to ensure cancel completes
            asyncio.create_task(
                old_client.stop()
            )  # Schedule stop to run in background
        else:
            # No current client, just create a new one
            self.current_client = self._create_new_client()
            asyncio.create_task(self.current_client.start())

        if self.ten_env:
            self.ten_env.log_info(
                "New TTS client instance created after cancel."
            )

    async def get(self, tts_input: TTSTextInput):
        """Delegate the get call to the current active client instance."""
        await self.current_client.get(tts_input)

    async def _cleanup_old_clients(self):
        """Periodically clean up old clients that have finished stopping."""
        while True:
            await asyncio.sleep(10)  # Check every 10 seconds
            stopped_clients = [
                client
                for client in self.old_clients
                if client.stopped_event.is_set()
            ]
            for client in stopped_clients:
                if self.ten_env:
                    self.ten_env.log_info(
                        f"Cleaning up stopped client: {id(client)}"
                    )
                self.old_clients.remove(client)
