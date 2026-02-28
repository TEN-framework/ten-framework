#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from abc import ABC, abstractmethod
import asyncio
from enum import Enum
import json
import traceback
import uuid

from .helper import AsyncQueue
from .message import (
    ModuleError,
    ModuleMetricKey,
    ModuleMetrics,
    ModuleType,
    TTSAudioEndReason,
)

from .struct import TTSFlush, TTSTextInput, TTSTextResult
from ten_runtime import (
    AsyncExtension,
    Data,
)
from ten_runtime.async_ten_env import AsyncTenEnv
from ten_runtime.audio_frame import AudioFrame, AudioFrameDataFmt
from ten_runtime.cmd import Cmd
from ten_runtime.cmd_result import CmdResult, StatusCode
from ten_ai_base.const import LOG_CATEGORY_VENDOR
from ten_ai_base.const import LOG_CATEGORY_KEY_POINT

DATA_TTS_TEXT_INPUT = "tts_text_input"
DATA_TTS_TEXT_RESULT = "tts_text_result"
DATA_FLUSH = "tts_flush"
DATA_FLUSH_RESULT = "tts_flush_end"
CMD_UPDATE_CONFIGS = "update_configs"


class RequestState(Enum):
    """
    Request lifecycle states:
    - QUEUED: Request received and queued, waiting for processing
    - PROCESSING: Actively processing text chunks (before text_input_end)
    - FINALIZING: All text received (text_input_end=True), generating final audio
    - COMPLETED: Terminal state - request finished (check TTSAudioEndReason for completion reason)
    """
    QUEUED = "queued"
    PROCESSING = "processing"
    FINALIZING = "finalizing"
    COMPLETED = "completed"


class AsyncTTS2BaseExtension(AsyncExtension, ABC):
    """
    Base class for implementing a Text-to-Speech Extension.
    This class provides a basic implementation for converting text to speech.
    It automatically handles the processing of tts requests.
    Use begin_send_audio_out, send_audio_out, end_send_audio_out to send the audio data to the output.
    Override on_request_tts to implement the TTS logic.
    """

    # Create the queue for message processing
    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None  # type: ignore
        self.input_queue = AsyncQueue()
        self.current_task = None
        self.loop_task = None
        self.leftover_bytes = b""
        self.session_id = None
        self.metadatas = {}
        self._flush_complete_event = asyncio.Event()  # allow get after flush is complete
        self._flush_complete_event.set()  # Initially allow gets
        self._put_lock = asyncio.Lock()  # Protect put operations from race conditions

        # State machine for request lifecycle management
        self.request_states: dict[str, RequestState] = {}
        # Tracks which request is currently being processed by the queue
        # None means no active request, allowing new requests to start
        # Reset to None when request completes (in finish_request) or flush occurs
        self._processing_request_id: str | None = None

        # Buffer for messages from different request_ids (to handle interleaved requests)
        # Key: request_id, Value: list of buffered messages for that request
        # Messages are buffered when they arrive while a different request is being processed
        self._pending_messages: dict[str, list[TTSTextInput]] = {}

        # metrics every 5 seconds
        self.output_characters = 0
        self.input_characters = 0
        self.recv_audio_duration = 0
        self.recv_audio_chunks_len = 0
        # metrics for request level
        self.total_output_characters = 0
        self.total_input_characters = 0
        self.total_recv_audio_duration = 0
        self.total_recv_audio_chunks_len = 0

        # Tracks which request_id's audio is currently being sent
        # Set in send_tts_audio_start(), reset in send_tts_audio_end() and flush
        # Used by send_tts_audio_data() to attach correct metadata to audio frames
        self.current_audio_request_id = None

    def _can_transition_to(self, request_id: str, new_state: RequestState) -> bool:
        """Check if state transition is valid."""
        current_state = self.request_states.get(request_id)
        if current_state is None:
            # No current state, can transition to QUEUED
            return new_state == RequestState.QUEUED

        # Define valid state transitions
        valid_transitions = {
            RequestState.QUEUED: [RequestState.PROCESSING, RequestState.COMPLETED],
            RequestState.PROCESSING: [RequestState.FINALIZING, RequestState.COMPLETED],
            RequestState.FINALIZING: [RequestState.COMPLETED],
            RequestState.COMPLETED: [],  # Terminal state, no transitions allowed
        }

        return new_state in valid_transitions.get(current_state, [])

    def _transition_state(self, request_id: str, new_state: RequestState, reason: str = "") -> bool:
        """
        Transition request to a new state with validation and logging.
        Returns True if transition succeeded, False otherwise.
        """
        current_state = self.request_states.get(request_id)

        if not self._can_transition_to(request_id, new_state):
            self.ten_env.log_warn(
                f"Invalid state transition for request {request_id}: "
                f"{current_state.value if current_state else 'None'} -> {new_state.value}"
            )
            return False

        self.request_states[request_id] = new_state
        log_msg = f"Request {request_id} state: {current_state.value if current_state else 'None'} -> {new_state.value}"
        if reason:
            log_msg += f" (reason: {reason})"
        self.ten_env.log_info(log_msg, category=LOG_CATEGORY_KEY_POINT)
        return True

    def _cleanup_completed_states(self) -> None:
        """
        Clean up old COMPLETED states to prevent unbounded memory growth.

        This is called when a new request arrives to ensure we don't accumulate
        COMPLETED states indefinitely. Flush operation will also clear all states.
        """
        completed_ids = [
            req_id for req_id, state in self.request_states.items()
            if state == RequestState.COMPLETED
        ]

        if completed_ids:
            for req_id in completed_ids:
                self.request_states.pop(req_id, None)
                # Metadata should already be cleaned up, but defensive cleanup
                self.metadatas.pop(req_id, None)

            self.ten_env.log_debug(
                f"Cleaned up {len(completed_ids)} COMPLETED states: {completed_ids}"
            )

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        await super().on_start(ten_env)
        if self.loop_task is None:
            self.loop = asyncio.get_event_loop()
            self.loop_task = self.loop.create_task(self._process_input_queue(ten_env))

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        # send laster time before stop
        await self.send_usage_metrics()
        await super().on_stop(ten_env)
        await self._flush_input_items()
        if self.loop_task:
            self.loop_task.cancel()
        await self.input_queue.put(None)  # Signal the loop to stop processing

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        await super().on_deinit(ten_env)

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd: {cmd_name}")

        if cmd_name == CMD_UPDATE_CONFIGS:
            try:
                content, err = cmd.get_property_to_json("")
                if err:
                    raise RuntimeError(f"Failed to get cmd property: {err}")

                configs = json.loads(content)
                await self.update_configs(configs)

                cmd_result = CmdResult.create(StatusCode.OK, cmd)
                await ten_env.return_result(cmd_result)
            except Exception as e:
                ten_env.log_error(f"update_configs failed: {e}")
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd)
                cmd_result.set_property_from_json(
                    "", json.dumps({"message": str(e)})
                )
                await ten_env.return_result(cmd_result)
        else:
            cmd_result = CmdResult.create(StatusCode.OK, cmd)
            await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        # Get the necessary properties
        data_name = data.get_name()
        ten_env.log_debug(f"on_data:{data_name}")

        if data.get_name() == DATA_TTS_TEXT_INPUT:
            data_payload, err = data.get_property_to_json("")
            if err:
                raise RuntimeError(f"Failed to get data payload: {err}")

            try:
                t = TTSTextInput.model_validate_json(data_payload)
                self.ten_env.log_info(
                    f"on_data tts_text_input:  {t}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
            except Exception as e:
                ten_env.log_warn(f"invalid data {data_name} payload, err {e}")
                return

            # Start an asynchronous task for handling tts
            # Wait for queue to be flushed before allowing new items to be queued
            # Use lock to prevent race condition between wait and put
            async with self._put_lock:
                await self._flush_complete_event.wait()
                
                # Record QUEUED state after flush check to avoid race condition
                # This ensures metadata is set atomically with queue put operation
                if t.request_id not in self.request_states:
                    # Clean up old COMPLETED states to prevent unbounded growth
                    self._cleanup_completed_states()

                    self._transition_state(t.request_id, RequestState.QUEUED, "request received")
                    if t.metadata:
                        self.metadatas[t.request_id] = t.metadata
                
                self.ten_env.log_debug(f"on_data tts_text_input put to queue")
                await self.input_queue.put(t)
        if data.get_name() == DATA_FLUSH:
            data_payload, err = data.get_property_to_json("")
            if err:
                raise RuntimeError(f"Failed to get data payload: {err}")

            try:
                t = TTSFlush.model_validate_json(data_payload)
            except Exception as e:
                ten_env.log_warn(f"invalid data {data_name} payload, err {e}")
                return
            ten_env.log_info(
                f"receive tts_flush {data_payload} ",
                category=LOG_CATEGORY_KEY_POINT,
            )
            # Use lock to prevent concurrent flush operations
            try:
                # Set flushing state to block put operations
                async with self._put_lock:
                    self._flush_complete_event.clear()

                await self._flush_input_items()

                flush_result = Data.create(DATA_FLUSH_RESULT)
                json_data = json.dumps(
                    {
                        "flush_id": t.flush_id,
                        "metadata": t.metadata,
                    }
                )
                flush_result.set_property_from_json(None, json_data)

                ten_env.log_info(
                    f"send tts_flush_end {json_data}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
                await ten_env.send_data(flush_result)

                # Complete flush process - allow get operations to resume
                self._flush_complete_event.set()
                ten_env.log_debug("on_data sent flush result")
            except Exception as e:
                # Ensure events are set even if flush fails
                self._flush_complete_event.set()
                ten_env.log_error(f"Flush operation failed: {e}")
                raise

    async def _flush_input_items(self):
        """Flushes the queue and cancels the current task."""
        # Flush the queue
        await self.input_queue.flush()

        # Clear buffered messages from different request_ids
        self._pending_messages.clear()

        # Cancel the current task if one is running
        if self._processing_request_id:
            current_id = self._processing_request_id
            current_state = self.request_states.get(current_id)

            if current_state and current_state != RequestState.COMPLETED:
                self.ten_env.log_info(
                    f"Cancelling current request {current_id} in state {current_state.value}",
                    category=LOG_CATEGORY_KEY_POINT,
                )

                await self._cancel_current_task()

                # Call cancel_tts() - subclass should send audio_end with reason=INTERRUPTED
                # Note: Subclass should NOT call finish_request(), state cleanup is handled below
                await self.cancel_tts()

        # Clear all request states and metadata
        # This handles both the current request and any queued requests
        self.request_states.clear()
        self.metadatas.clear()

        # Reset processing request ID and current audio request ID
        self._processing_request_id = None
        self.current_audio_request_id = None

        self.ten_env.log_debug("Cleared all request states, metadata, and pending messages after flush")


    async def _cancel_current_task(self) -> None:
        """Called when the TTS request is cancelled."""
        if self.current_task:
            self.current_task.cancel()
            self.current_task = None
        self.leftover_bytes = b""

    async def _process_input_queue(self, ten_env: AsyncTenEnv) -> None:
        """
        Asynchronously process queue items one by one.

        Handles out-of-order messages by buffering messages from different request_ids.
        Example: If messages arrive as [req3, req3, req4, req4, req3(end)],
        req4 messages will be buffered until req3 completes.
        """
        while True:
            # Wait for an item to be available in the queue
            t: TTSTextInput = await self.input_queue.get()
            if t is None:
                break

            # If we're currently processing a different request, buffer this message
            if (
                self._processing_request_id is not None
                and t.request_id != self._processing_request_id
            ):
                # Buffer the message for later processing
                if t.request_id not in self._pending_messages:
                    self._pending_messages[t.request_id] = []
                self._pending_messages[t.request_id].append(t)

                ten_env.log_debug(
                    f"Buffered message for request {t.request_id} (currently processing {self._processing_request_id}), "
                    f"buffer size: {len(self._pending_messages[t.request_id])}"
                )
                continue
            elif (
                self._processing_request_id is None
                and t.request_id not in self._pending_messages
            ):
                # Message arrived when no request is processing and this request has no buffered messages
                # This is normal - the message will be processed immediately
                ten_env.log_debug(
                    f"Processing message for request {t.request_id} immediately "
                    f"(no active request, no buffered messages for this request)"
                )

            # Start processing a new request or continue processing current request
            # This handles two cases:
            # 1. _processing_request_id is None (no active request) - start new request
            # 2. _processing_request_id == t.request_id (continue current request) - no state change needed
            if self._processing_request_id != t.request_id:
                self._processing_request_id = t.request_id
                self._transition_state(t.request_id, RequestState.PROCESSING, "start processing")

            try:
                if t.text_input_end:
                    self._transition_state(
                        t.request_id, RequestState.FINALIZING,
                        "received text_input_end, generating final audio"
                    )
                await self.request_tts(t)

            except asyncio.CancelledError:
                ten_env.log_info(f"Task cancelled: {t.text}")
            except Exception as err:
                ten_env.log_error(
                    f"Task failed: {t.text}, err: {traceback.format_exc()}"
                )

    async def send_tts_audio_data(self, audio_data: bytes, timestamp: int = 0) -> None:
        """End sending audio out."""
        try:
            sample_rate = self.synthesize_audio_sample_rate()
            bytes_per_sample = self.synthesize_audio_sample_width()
            number_of_channels = self.synthesize_audio_channels()
            # Combine leftover bytes with new audio data
            combined_data = self.leftover_bytes + audio_data

            # Check if combined_data length is odd
            if len(combined_data) % (bytes_per_sample * number_of_channels) != 0:
                # Save the last incomplete frame
                valid_length = len(combined_data) - (
                    len(combined_data) % (bytes_per_sample * number_of_channels)
                )
                self.leftover_bytes = combined_data[valid_length:]
                combined_data = combined_data[:valid_length]
            else:
                self.leftover_bytes = b""

            if combined_data:
                f = AudioFrame.create("pcm_frame")
                f.set_sample_rate(sample_rate)
                f.set_bytes_per_sample(bytes_per_sample)
                f.set_number_of_channels(number_of_channels)
                f.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
                f.set_samples_per_channel(
                    len(combined_data) // (bytes_per_sample * number_of_channels)
                )
                f.alloc_buf(len(combined_data))
                f.set_timestamp(timestamp)
                f.set_property_from_json("metadata", json.dumps(self.metadatas.get(self.current_audio_request_id, {})))
                buff = f.lock_buf()
                buff[:] = combined_data
                f.unlock_buf(buff)
                await self.ten_env.send_audio_frame(f)
        except Exception as e:
            self.ten_env.log_error(f"error send audio frame, {traceback.format_exc()}")

    async def send_tts_text_result(self, t: TTSTextResult) -> None:
        data = Data.create(DATA_TTS_TEXT_RESULT)
        data.set_property_from_json("", t.model_dump_json())
        await self.ten_env.send_data(data)

    async def send_tts_ttfb_metrics(
        self,
        request_id: str,
        ttfb_ms: int,
        turn_id: int = -1,
        extra_metadata: dict | None = None,
    ) -> None:
        # if there is extra metadata, add it to the basic metadata
        new_metadata = self.update_metadata(request_id, extra_metadata)

        metrics = ModuleMetrics(
            id=self.get_uuid(),
            module=ModuleType.TTS,
            vendor=self.vendor(),
            metrics={ModuleMetricKey.TTS_TTFB: ttfb_ms},
            metadata=new_metadata,
        )
        self.ten_env.log_info(
            f"tts_ttfb:  {ttfb_ms} of request_id: {request_id}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        await self.send_metrics(metrics, request_id)

    async def send_tts_audio_start(
        self, request_id: str, turn_id: int = -1, extra_metadata: dict | None = None
    ) -> None:
        # Set current_audio_request_id to track which request's audio is being sent
        self.current_audio_request_id = request_id

        new_metadata = self.update_metadata(request_id, extra_metadata)

        data = Data.create("tts_audio_start")
        json_data = json.dumps(
            {
                "request_id": request_id,
                "metadata": new_metadata,
            }
        )
        data.set_property_from_json(None, json_data)
        self.ten_env.log_info(
            f"tts_audio_start:  {json_data} of request_id: {request_id}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        await self.ten_env.send_data(data)

    async def send_tts_audio_end(
        self,
        request_id: str,
        request_event_interval_ms: int,
        request_total_audio_duration_ms: int,
        turn_id: int = -1,
        reason: TTSAudioEndReason = TTSAudioEndReason.REQUEST_END,
        extra_metadata: dict | None = None,
    ) -> None:
        new_metadata = self.update_metadata(request_id, extra_metadata)
        data = Data.create("tts_audio_end")
        json_data = json.dumps(
            {
                "request_id": request_id,
                "request_event_interval_ms": request_event_interval_ms,
                "request_total_audio_duration_ms": request_total_audio_duration_ms,
                "reason": reason.value,
                "metadata": new_metadata,
            }
        )
        data.set_property_from_json(None, json_data)
        self.ten_env.log_info(
            f"tts_audio_end:  {json_data} of request_id: {request_id}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        await self.ten_env.send_data(data)
 
        # Reset current_audio_request_id (audio phase complete)
        if self.current_audio_request_id == request_id:
            self.current_audio_request_id = None

    async def send_tts_error(
        self,
        request_id: str | None,
        error: ModuleError,
        turn_id: int = -1,
        extra_metadata: dict | None = None,
    ) -> None:
        new_metadata = self.update_metadata(request_id, extra_metadata)
        """
        Send an error message related to TTS processing.
        """
        error_data = Data.create("error")

        vendor_info = error.vendor_info
        vendorInfo = None
        if vendor_info:
            vendorInfo = {
                "vendor": vendor_info.vendor,
                "code": vendor_info.code,
                "message": vendor_info.message,
            }
        json_data = json.dumps(
            {
                "id": request_id or "",
                "code": error.code,
                "module": ModuleType.TTS,
                "message": error.message,
                "vendor_info": vendorInfo or {},
                "metadata": new_metadata,
            }
        )
        error_data.set_property_from_json(
            None,
            json_data,
        )
        self.ten_env.log_error(
            f"tts_error:  msg: 	{json_data}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        await self.ten_env.send_data(error_data)

    async def send_char_audio_metrics(self, request_id: str = ""):
        await self.send_usage_metrics(request_id)

    # send when tts audio end
    async def send_usage_metrics(
        self, request_id: str = "", extra_metadata: dict | None = None
    ):
        new_metadata = self.update_metadata(request_id, extra_metadata)
        await self.metrics_calculate_duration()
        metrics = ModuleMetrics(
            id=self.get_uuid(),
            module=ModuleType.TTS,
            vendor=self.vendor(),
            metrics={
                "output_characters": self.output_characters,
                "input_characters": self.input_characters,
                "recv_audio_duration": self.recv_audio_duration,
                "total_output_characters": self.total_output_characters,
                "total_input_characters": self.total_input_characters,
                "total_recv_audio_duration": self.total_recv_audio_duration,
            },
            metadata=new_metadata,
        )
        await self.send_metrics(metrics, request_id)
        self.metrics_reset()

    async def send_metrics(self, metrics: ModuleMetrics, request_id: str = ""):
        data = Data.create("metrics")
        self.ten_env.log_info(
            f"tts_metrics:  {metrics} of request_id: {request_id}",
            category=LOG_CATEGORY_KEY_POINT,
        )
        data.set_property_from_json(None, metrics.model_dump_json())
        await self.ten_env.send_data(data)

    async def metrics_calculate_duration(self) -> None:
        self.recv_audio_duration = (
            self.recv_audio_chunks_len
            / self.synthesize_audio_channels()
            * 1000
            / self.synthesize_audio_sample_width()
        ) / self.synthesize_audio_sample_rate()
        self.total_recv_audio_duration = (
            self.total_recv_audio_chunks_len
            / self.synthesize_audio_channels()
            * 1000
            / self.synthesize_audio_sample_width()
        ) / self.synthesize_audio_sample_rate()

    def metrics_add_output_characters(self, characters: int) -> None:
        self.output_characters += characters
        self.total_output_characters += characters

    def metrics_add_input_characters(self, characters: int) -> None:
        self.input_characters += characters
        self.total_input_characters += characters

    def metrics_add_recv_audio_chunks(self, chunks: bytes) -> None:
        self.total_recv_audio_chunks_len += len(chunks)
        self.recv_audio_chunks_len += len(chunks)

    def metrics_reset(self) -> None:
        self.output_characters = 0
        self.input_characters = 0
        self.recv_audio_duration = 0
        self.recv_audio_chunks_len = 0

    async def metrics_connect_delay(
        self, connect_delay_ms: int, extra_metadata: dict | None = None, request_id: str = ""
    ):
        new_metadata = self.update_metadata(request_id, extra_metadata)
        metrics = ModuleMetrics(
            id=self.get_uuid(),
            module=ModuleType.TTS,
            vendor=self.vendor(),
            metrics={
                "connect_delay": connect_delay_ms,
            },
            metadata=new_metadata,
        )
        await self.send_metrics(metrics, request_id)
        self.ten_env.log_debug(f"metrics_connect_delay: {metrics}")

    async def finish_request(
        self,
        request_id: str,
        reason: TTSAudioEndReason = TTSAudioEndReason.REQUEST_END,
        error: ModuleError | None = None,
    ) -> None:
        """
        Finish a TTS request with proper state transition and cleanup.

        This method handles:
        1. Send error message (if error provided)
        2. Transition to COMPLETED state
        3. Clean up request metadata
        4. Reset _processing_request_id to allow next request
        5. Signal request finished event
        6. Release buffered messages from other requests (for interleaved requests)

        NOTE: This method does NOT send audio_end event. Subclasses should:
        1. Send audio_end event themselves (via send_tts_audio_end)
        2. Then call finish_request() to complete state transition

        Usage examples:
        - Normal completion:
            await self.send_tts_audio_end(id, interval_ms, duration_ms, reason=REQUEST_END)
            await self.finish_request(id, reason=TTSAudioEndReason.REQUEST_END)
        - Error completion:
            await self.finish_request(id, reason=TTSAudioEndReason.ERROR, error=ModuleError(...))
        - Interrupted:
            await self.send_tts_audio_end(id, interval_ms, duration_ms, reason=INTERRUPTED)
            await self.finish_request(id, reason=TTSAudioEndReason.INTERRUPTED)

        Args:
            request_id: The request ID to finish
            reason: Why the request is ending (default: TTSAudioEndReason.REQUEST_END)
            error: Optional error to report before finishing
        """
        # Send error message if provided
        if error:
            await self.send_tts_error(request_id, error)
            self.ten_env.log_info(
                f"Finishing request {request_id} with error: {error.message}",
                category=LOG_CATEGORY_KEY_POINT,
            )

        # Transition to COMPLETED state
        reason_str = f"request finished with reason={reason.value}"
        self._transition_state(request_id, RequestState.COMPLETED, reason_str)

        # Keep the COMPLETED state for observability
        # State will be cleaned up by:
        # 1. _flush_input_items() when flush is called
        # 2. _cleanup_completed_states() when starting a new request (to prevent unbounded growth)

        # Defensive reset of current_audio_request_id for error paths
        if self.current_audio_request_id == request_id:
            self.current_audio_request_id = None

        # Handle request completion and buffered messages release
        # Only process if this is the currently processing request
        if self._processing_request_id == request_id:
            # IMPORTANT: Reset _processing_request_id BEFORE releasing buffered messages
            # This ensures that when buffered messages are put back to queue and immediately
            # processed by _process_input_queue, they won't be buffered again
            self._processing_request_id = None

            # Release buffered messages from other requests (for interleaved requests)
            # Only release one request at a time to maintain order
            if self._pending_messages:
                # Find the next request_id with buffered messages
                next_request_ids = list(self._pending_messages.keys())
                self.ten_env.log_info(
                    f"Request {request_id} finished, _pending_messages keys (in order): {next_request_ids}, "
                    f"will release first: {next_request_ids[0] if next_request_ids else 'None'}",
                    category=LOG_CATEGORY_KEY_POINT,
                )
                if next_request_ids:
                    next_request_id = next_request_ids[0]
                    buffered_messages = self._pending_messages.pop(next_request_id)

                    self.ten_env.log_info(
                        f"Request {request_id} finished, releasing buffered request {next_request_id} "
                        f"with {len(buffered_messages)} messages. "
                        f"Remaining pending messages keys: {list(self._pending_messages.keys())}",
                        category=LOG_CATEGORY_KEY_POINT,
                    )

                    # IMPORTANT: Set _processing_request_id BEFORE putting messages back to queue
                    # This prevents race condition where _process_input_queue processes messages
                    # before _processing_request_id is set, which could cause incorrect buffering
                    self._processing_request_id = next_request_id
                    self.ten_env.log_debug(
                        f"Set _processing_request_id to {next_request_id} before releasing messages"
                    )

                    # Put buffered messages back to queue (in order)
                    # Use _put_lock to prevent race condition with on_data
                    # This ensures all buffered messages are enqueued atomically
                    async with self._put_lock:
                        for msg in buffered_messages:
                            await self.input_queue.put(msg)
                            self.ten_env.log_debug(
                                f"Put buffered message back to queue: request_id={msg.request_id}, "
                                f"text={msg.text[:50]}..."
                            )

    @abstractmethod
    def vendor(self) -> str:
        """
        Get the vendor name of the TTS implementation.
        This is used for metrics and error reporting.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def request_tts(self, t: TTSTextInput) -> None:
        """
        Called when a new input item is available in the queue. Override this method to implement the TTS request logic.
        Use send_audio_out to send the audio data to the output when the audio data is ready.
        """
        raise NotImplementedError("request_tts must be implemented in the subclass")

    @abstractmethod
    def synthesize_audio_sample_rate(self) -> int:
        """
        Get the input audio sample rate in Hz.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    def synthesize_audio_channels(self) -> int:
        """
        Get the number of audio channels for input.
        Default is 1 (mono).
        """
        return 1

    def synthesize_audio_sample_width(self) -> int:
        """
        Get the sample width in bytes for input audio.
        Default is 2 (16-bit PCM).
        """
        return 2

    def get_uuid(self) -> str:
        """
        Get a unique identifier
        """
        return uuid.uuid4().hex

    def update_metadata(self, request_id: str| None, metadata: dict | None) -> dict:
        new_metadata = {}
        if request_id and request_id in self.metadatas:
            new_metadata = self.metadatas.get(request_id).copy()
        if metadata:
            new_metadata.update(metadata)
        return new_metadata

    async def cancel_tts(self) -> None:
        """
        Called when a flush request is received. Override this method to implement TTS-specific cancellation logic.

        Subclass responsibilities:
        1. Cancel any ongoing vendor API operations
        2. Send audio_end with reason=INTERRUPTED (if there's an active request)
        3. Do NOT call finish_request() - state cleanup is handled by the base class

        This method is called after flushing the input queue and cancelling the current task.
        Implementations should be fast to avoid blocking.
        """
        pass

    async def update_configs(self, configs: dict) -> None:
        """
        Called when update_configs command is received.
        Subclasses should override this method to handle config updates.
        """
        pass
