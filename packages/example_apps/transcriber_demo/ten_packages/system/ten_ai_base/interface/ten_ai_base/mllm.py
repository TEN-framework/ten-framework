#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from abc import abstractmethod
import traceback
from typing import final
import uuid

from .struct import MLLMClientCreateResponse, MLLMClientFunctionCallOutput, MLLMClientMessageItem, MLLMClientRegisterTool, MLLMClientSendMessageItem, MLLMClientSetMessageContext, MLLMServerFunctionCall, MLLMServerInputTranscript, MLLMServerOutputTranscript, MLLMServerSessionReady, MLLMServerInterrupt

from .types import LLMToolMetadata, MLLMBufferConfig, MLLMBufferConfigModeDiscard, MLLMBufferConfigModeKeep

from .message import (
    ModuleError,
    ModuleErrorVendorInfo,
    ModuleType,
)
from .const import (
    PROPERTY_KEY_METADATA,
    PROPERTY_KEY_SESSION_ID,
)
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    AudioFrameDataFmt,
    Cmd,
    Data,
    AudioFrame,
    StatusCode,
    CmdResult,
)
import asyncio
import json

DATA_MLLM_OUT_SESSION_READY = "mllm_server_session_ready"
DATA_MLLM_OUT_INTERRUPTED = "mllm_server_interrupted"
DATA_MLLM_OUT_REQUEST_TRANSCRIPT = "mllm_server_input_transcript"
DATA_MLLM_OUT_RESPONSE_TRANSCRIPT = "mllm_server_output_transcript"
DATA_MLLM_OUT_FUNCTION_CALL = "mllm_server_function_call"
DATA_MLLM_IN_SET_MESSAGE_CONTEXT = "mllm_client_set_message_context"
DATA_MLLM_IN_SEND_MESSAGE_ITEM = "mllm_client_request_message_item"
DATA_MLLM_IN_CREATE_RESPONSE = "mllm_client_request_create_response"
DATA_MLLM_IN_REGISTER_TOOL = "mllm_client_request_register_tool"
DATA_MLLM_IN_FUNCTION_CALL_OUTPUT = "mllm_client_function_call_output"


class AsyncMLLMBaseExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.stopped = False
        self.ten_env: AsyncTenEnv = None  # type: ignore
        self.session_id = None
        self.sent_buffer_length = 0
        self.buffered_frames = asyncio.Queue[AudioFrame]()
        self.buffered_frames_size = 0
        self.audio_frames_queue = asyncio.Queue[AudioFrame]()
        self.uuid = self._get_uuid()  # Unique identifier for the current final turn
        self.leftover_bytes = b""
        self.message_context: list[MLLMClientMessageItem] = []  # Context for the current message

        # States for TTFW calculation
        self.first_audio_time: float | None = (
            None  # Record the timestamp of the first audio send
        )

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        asyncio.create_task(self._audio_frame_consumer())

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_start")
        await self.start_connection()

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        await self.audio_frames_queue.put(audio_frame)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_debug(f"on_data name: {data_name}")
        if data_name == DATA_MLLM_IN_SEND_MESSAGE_ITEM:
            send_message, _ = data.get_property_to_json(None)
            message_item = MLLMClientSendMessageItem.model_validate_json(send_message)
            await self.send_client_message_item(message_item.message, self.session_id)
        elif data_name == DATA_MLLM_IN_CREATE_RESPONSE:
            create_response, _ = data.get_property_to_json(None)
            create_response = MLLMClientCreateResponse.model_validate_json(create_response)
            await self.send_client_create_response(self.session_id)
        elif data_name == DATA_MLLM_IN_SET_MESSAGE_CONTEXT:
            set_message_context, _ = data.get_property_to_json(None)
            messages = MLLMClientSetMessageContext.model_validate_json(set_message_context).messages
            self.message_context = messages
        elif data_name == DATA_MLLM_IN_REGISTER_TOOL:
            register_tool_json, _ = data.get_property_to_json(None)
            register_tool = MLLMClientRegisterTool.model_validate_json(register_tool_json)
            await self.send_client_register_tool(register_tool.tool)
        elif data_name == DATA_MLLM_IN_FUNCTION_CALL_OUTPUT:
            function_call_output_json, _ = data.get_property_to_json(None)
            function_call_output = MLLMClientFunctionCallOutput.model_validate_json(function_call_output_json)
            await self.send_client_function_call_output(function_call_output)


    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop")

        self.stopped = True

        await self.stop_connection()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd json: {cmd_name}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    @abstractmethod
    def vendor(self) -> str:
        """Get the name of the MLLM vendor."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def start_connection(self) -> None:
        """Start the connection to the MLLM service."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    def is_connected(self) -> bool:
        """Check if the MLLM service is connected."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def stop_connection(self) -> None:
        """Stop the connection to the MLLM service."""
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        """
        Send a message item to the MLLM service.
        This method is used to send user or assistant messages.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        """
        Send a create response to the MLLM service.
        This method is used to trigger MLLM to generate a response.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def send_client_register_tool(
        self, tools: LLMToolMetadata
    ) -> None:
        """
        Register tools with the MLLM service.
        This method is used to register tools that can be called by the LLM.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        """
        Send a function call output to the MLLM service.
        This method is used to send the result of a function call made by the LLM.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @abstractmethod
    def input_audio_sample_rate(self) -> int:
        """
        Get the input audio sample rate in Hz.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    def input_audio_channels(self) -> int:
        """
        Get the number of audio channels for input.
        Default is 1 (mono).
        """
        return 1

    def input_audio_sample_width(self) -> int:
        """
        Get the sample width in bytes for input audio.
        Default is 2 (16-bit PCM).
        """
        return 2


    @abstractmethod
    def synthesize_audio_sample_rate(self) -> int:
        """
        Get the input audio sample rate in Hz.
        """
        return 24000

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

    def buffer_strategy(self) -> MLLMBufferConfig:
        """
        Get the buffer strategy for audio frames when not connected
        """
        return MLLMBufferConfigModeDiscard()

    @abstractmethod
    async def send_audio(self, frame: AudioFrame, session_id: str | None) -> bool:
        """
        Send an audio frame to the MLLM service, returning True if successful.
        Note: The first successful send_audio call will be timestamped for TTFW calculation.
        """
        raise NotImplementedError("This method should be implemented in subclasses.")

    @final
    async def send_server_session_ready(self, session: MLLMServerSessionReady) -> None:
        """
        Send a session ready event to the MLLM server.
        This is typically used to notify that the session is ready for processing.
        """
        if self.session_id is not None:
            session.metadata[PROPERTY_KEY_SESSION_ID] = self.session_id
        data = Data.create(DATA_MLLM_OUT_SESSION_READY)
        data.set_property_from_json("", session.model_dump_json())
        await self.ten_env.send_data(data)

    @final
    async def send_server_interrupted(self, sos: MLLMServerInterrupt) -> None:
        """
        Send a start of speech event to the MLLM server.
        This is typically used to notify that the user has started speaking.
        """
        if self.session_id is not None:
            sos.metadata[PROPERTY_KEY_SESSION_ID] = self.session_id
        data = Data.create(DATA_MLLM_OUT_INTERRUPTED)
        data.set_property_from_json("", sos.model_dump_json())
        await self.ten_env.send_data(data)

    @final
    async def send_server_input_transcript(self, request_transcript: MLLMServerInputTranscript) -> None:
        """
        Send a transcription result as output.
        """
        if self.session_id is not None:
            request_transcript.metadata[PROPERTY_KEY_SESSION_ID] = self.session_id

        stable_data = Data.create(DATA_MLLM_OUT_REQUEST_TRANSCRIPT)

        model_json = request_transcript.model_dump()

        stable_data.set_property_from_json(
            None,
            json.dumps(model_json),
        )

        await self.ten_env.send_data(stable_data)

        if request_transcript.final:
            self.uuid = self._get_uuid()  # Reset UUID for the next final turn


    async def send_server_output_audio_data(
        self, audio_data: bytes
    ) -> None:
        """End sending audio out."""
        try:
            sample_rate = self.synthesize_audio_sample_rate()
            bytes_per_sample = self.synthesize_audio_sample_width()
            number_of_channels = self.synthesize_audio_channels()
            # Combine leftover bytes with new audio data
            combined_data = self.leftover_bytes + audio_data

            # Check if combined_data length is odd
            if (
                len(combined_data) % (bytes_per_sample * number_of_channels)
                != 0
            ):
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
                    len(combined_data)
                    // (bytes_per_sample * number_of_channels)
                )
                f.alloc_buf(len(combined_data))
                buff = f.lock_buf()
                buff[:] = combined_data
                f.unlock_buf(buff)
                await self.ten_env.send_audio_frame(f)
        except Exception as e:
            self.ten_env.log_error(
                f"error send audio frame, {traceback.format_exc()}"
            )

    @final
    async def send_server_output_text(
        self, t: MLLMServerOutputTranscript
    ) -> None:
        if self.session_id is not None:
            t.metadata[PROPERTY_KEY_SESSION_ID] = self.session_id
        data = Data.create(DATA_MLLM_OUT_RESPONSE_TRANSCRIPT)
        data.set_property_from_json("", t.model_dump_json())
        await self.ten_env.send_data(data)

    @final
    async def send_server_function_call(
        self, function_call: MLLMServerFunctionCall
    ) -> None:
        """
        Send a function call event to the MLLM server.
        This is typically used to notify that a function call has been made by the MLLM server.
        """
        if self.session_id is not None:
            function_call.metadata[PROPERTY_KEY_SESSION_ID] = self.session_id
        data = Data.create(DATA_MLLM_OUT_FUNCTION_CALL)
        data.set_property_from_json("", function_call.model_dump_json())
        await self.ten_env.send_data(data)

    @final
    async def send_mllm_error(
        self, error: ModuleError, vendor_info: ModuleErrorVendorInfo | None = None
    ) -> None:
        """
        Send an error message related to MLLM processing.
        """
        error_data = Data.create("error")

        vendorInfo = None
        if vendor_info:
            vendorInfo = {
                "vendor": vendor_info.vendor,
                "code": vendor_info.code,
                "message": vendor_info.message,
            }

        error_data.set_property_from_json(
            None,
            json.dumps(
                {
                    "id": self.uuid,
                    "module": ModuleType.MLLM,
                    "code": error.code,
                    "message": error.message,
                    "vendor_info": vendorInfo or {},
                    "metadata": (
                        {}
                        if self.session_id is None
                        else {PROPERTY_KEY_SESSION_ID: self.session_id}
                    ),
                }
            ),
        )

        await self.ten_env.send_data(error_data)


    def _get_uuid(self) -> str:
        """
        Get a unique identifier
        """
        return uuid.uuid4().hex

    async def _handle_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        frame_buf = audio_frame.get_buf()
        if not frame_buf:
            ten_env.log_warn("send_frame: empty pcm_frame detected.")
            return

        if not self.is_connected():
            ten_env.log_debug("send_frame: service not connected.")
            buffer_strategy = self.buffer_strategy()
            if isinstance(buffer_strategy, MLLMBufferConfigModeKeep):
                byte_limit = buffer_strategy.byte_limit
                while self.buffered_frames_size + len(frame_buf) > byte_limit:
                    if self.buffered_frames.empty():
                        break
                    discard_frame = await self.buffered_frames.get()
                    self.buffered_frames_size -= len(discard_frame.get_buf())
                self.buffered_frames.put_nowait(audio_frame)
                self.buffered_frames_size += len(frame_buf)
            # return anyway if not connected
            return

        metadata, _ = audio_frame.get_property_to_json(PROPERTY_KEY_METADATA)
        if metadata:
            try:
                metadata_json = json.loads(metadata)
                self.session_id = metadata_json.get(
                    PROPERTY_KEY_SESSION_ID, self.session_id
                )
            except json.JSONDecodeError as e:
                ten_env.log_warn(f"send_frame: invalid metadata json - {e}")

        if self.buffered_frames.qsize() > 0:
            ten_env.log_debug(
                f"send_frame: flushing {self.buffered_frames.qsize()} buffered frames."
            )
            while True:
                try:
                    buffered_frame = self.buffered_frames.get_nowait()
                    await self.send_audio(buffered_frame, self.session_id)
                except asyncio.QueueEmpty:
                    break
            self.buffered_frames_size = 0

        success = await self.send_audio(audio_frame, self.session_id)

        if success:
            self.sent_buffer_length += len(frame_buf)

            # Record the timestamp of the first successful send_audio call for TTFW calculation.
            if self.first_audio_time is None:
                self.first_audio_time = asyncio.get_event_loop().time()

    async def _audio_frame_consumer(self) -> None:
        while not self.stopped:
            try:
                audio_frame = await self.audio_frames_queue.get()
                await self._handle_audio_frame(self.ten_env, audio_frame)
            except asyncio.CancelledError:
                break
            except Exception as e:
                self.ten_env.log_error(f"Error consuming audio frame: {e}")
