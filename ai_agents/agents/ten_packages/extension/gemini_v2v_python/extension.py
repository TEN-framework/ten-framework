#
#
# Agora Real Time Engagement
# Created by Wei Hu in 2024-08.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
import asyncio
from enum import Enum
import json
import traceback
import time
from google import genai
import numpy as np
from typing import Iterable, cast, Optional

import websockets

from ten import (
    AudioFrame,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from ten.audio_frame import AudioFrameDataFmt
from ten_ai_base.const import CMD_PROPERTY_RESULT, CMD_TOOL_CALL
from dataclasses import dataclass
from ten_ai_base.config import BaseConfig
from ten_ai_base.chat_memory import ChatMemory
from ten_ai_base.usage import (
    LLMUsage,
    LLMCompletionTokensDetails,
    LLMPromptTokensDetails,
)
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolResult,
    LLMChatCompletionContentPartParam,
    TTSPcmOptions,
)
from ten_ai_base.llm import AsyncLLMBaseExtension
from google.genai.types import (
    LiveServerMessage,
    LiveConnectConfig,
    LiveConnectConfigDict,
    GenerationConfig,
    Content,
    Part,
    Tool,
    FunctionDeclaration,
    Schema,
    LiveClientToolResponse,
    FunctionCall,
    FunctionResponse,
    SpeechConfig,
    VoiceConfig,
    PrebuiltVoiceConfig,
    StartSensitivity,
    EndSensitivity,
    AutomaticActivityDetection,
    RealtimeInputConfig,
    AudioTranscriptionConfig,
    ProactivityConfig,
    LiveServerContent,
    Modality,
    MediaResolution,
)
from google.genai.live import AsyncSession
from google.genai import types
from PIL import Image
from io import BytesIO
from base64 import b64encode

import urllib.parse
import google.genai._api_client

google.genai._api_client.urllib = urllib  # pylint: disable=protected-access

CMD_IN_FLUSH = "flush"
CMD_IN_ON_USER_JOINED = "on_user_joined"
CMD_IN_ON_USER_LEFT = "on_user_left"
CMD_OUT_FLUSH = "flush"


class Role(str, Enum):
    User = "user"
    Assistant = "assistant"


def rgb2base64jpeg(rgb_data, width, height):
    # Convert the RGB image to a PIL Image
    pil_image = Image.frombytes("RGBA", (width, height), bytes(rgb_data))
    pil_image = pil_image.convert("RGB")

    # Resize the image while maintaining its aspect ratio
    pil_image = resize_image_keep_aspect(pil_image, 512)

    # Save the image to a BytesIO object in JPEG format
    buffered = BytesIO()
    pil_image.save(buffered, format="JPEG")
    # pil_image.save("test.jpg", format="JPEG")

    # Get the byte data of the JPEG image
    jpeg_image_data = buffered.getvalue()

    # Convert the JPEG byte data to a Base64 encoded string
    base64_encoded_image = b64encode(jpeg_image_data).decode("utf-8")

    # Create the data URL
    # mime_type = "image/jpeg"
    return base64_encoded_image


def resize_image_keep_aspect(image, max_size=512):
    """
    Resize an image while maintaining its aspect ratio, ensuring the larger dimension is max_size.
    If both dimensions are smaller than max_size, the image is not resized.

    :param image: A PIL Image object
    :param max_size: The maximum size for the larger dimension (width or height)
    :return: A PIL Image object (resized or original)
    """
    # Get current width and height
    width, height = image.size

    # If both dimensions are already smaller than max_size, return the original image
    if width <= max_size and height <= max_size:
        return image

    # Calculate the aspect ratio
    aspect_ratio = width / height

    # Determine the new dimensions
    if width > height:
        new_width = max_size
        new_height = int(max_size / aspect_ratio)
    else:
        new_height = max_size
        new_width = int(max_size * aspect_ratio)

    # Resize the image with the new dimensions
    resized_image = image.resize((new_width, new_height))

    return resized_image


@dataclass
class GeminiRealtimeConfig(BaseConfig):
    base_uri: str = ""
    api_key: str = ""
    api_version: str = ""
    model: str = "models/gemini-2.5-flash-preview-native-audio-dialog"
    language: str = "en-US"
    prompt: str = ""
    temperature: float = 0.5
    max_tokens: int = 1024
    voice: str = "Puck"
    server_vad: bool = True
    audio_out: bool = True
    input_transcript: bool = True
    sample_rate: int = 24000
    stream_id: int = 0
    dump: bool = False
    greeting: str = "hello"
    # Audio optimization settings
    audio_buffer_threshold: int = 1024
    # Transcription settings
    transcribe_agent: bool = False
    transcribe_user: bool = False
    # Dialog features settings
    affective_dialog: bool = False
    proactive_audio: bool = False
    # VAD settings
    start_of_speech_sensitivity: Optional[str] = None
    end_of_speech_sensitivity: Optional[str] = None
    prefix_padding_ms: Optional[int] = None
    silence_duration_ms: Optional[int] = None

    media_resolution: MediaResolution = MediaResolution.MEDIA_RESOLUTION_MEDIUM
    context_window_trigger_tokens: int = 25600
    context_window_sliding_window_target_tokens: int = 12800

    def build_ctx(self) -> dict:
        return {
            "language": self.language,
            "model": self.model,
        }


class GeminiRealtimeExtension(AsyncLLMBaseExtension):
    def __init__(self, name):
        super().__init__(name)
        self.config: GeminiRealtimeConfig = None
        self.stopped: bool = False
        self.connected: bool = False
        self.buffer: bytearray = bytearray()
        self.memory: ChatMemory = None
        self.total_usage: LLMUsage = LLMUsage()
        self.users_count = 0

        self.stream_id: int = 0
        self.remote_stream_id: int = 0
        self.channel_name: str = ""
        self.audio_len_threshold: int = 1024  # Will be updated from config

        self.completion_times = []
        self.connect_times = []
        self.first_token_times = []

        self.buff: bytearray = bytearray()
        self.transcript: str = ""
        self.ctx: dict = {}
        self.input_end = time.time()
        self.client = None
        self.session: AsyncSession = None
        self.leftover_bytes = b""
        self.video_task = None
        self.image_queue = asyncio.Queue(maxsize=5)
        self.audio_queue = asyncio.Queue(maxsize=10)
        self.video_buff: str = ""
        self.loop = None
        self.ten_env = None
        self.last_audio_time = time.time()  # Track last audio received
        self.tasks = []

        # Cache for session configuration to reduce cold start time
        self._cached_session_config = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        await super().on_start(ten_env)
        self.ten_env = ten_env
        ten_env.log_debug("on_start")

        self.loop = asyncio.get_event_loop()

        self.config = await GeminiRealtimeConfig.create_async(ten_env=ten_env)
        ten_env.log_info(f"config: {self.config}")

        # Update audio threshold from config
        self.audio_len_threshold = self.config.audio_buffer_threshold

        if not self.config.api_key:
            ten_env.log_error("api_key is required")
            return

        try:
            self.ctx = self.config.build_ctx()
            self.ctx["greeting"] = self.config.greeting

            self.client = genai.Client(
                api_key=self.config.api_key,
            )

            self.tasks = []
            self.tasks.append(
                self.loop.create_task(self._connection_manager(ten_env))
            )
            self.tasks.append(self.loop.create_task(self._on_video(ten_env)))
            self.tasks.append(
                self.loop.create_task(self._process_audio_queue(ten_env))
            )

        except Exception as e:
            traceback.print_exc()
            self.ten_env.log_error(f"Failed to init client {e}")

    async def _connection_manager(self, ten_env: AsyncTenEnv) -> None:
        """Manage connection with retries and proper error handling."""
        retry_count = 0
        max_retries = 5
        base_retry_delay = 1.0  # seconds

        while not self.stopped:
            try:
                # If we've hit max retries, wait longer before trying again
                if retry_count >= max_retries:
                    ten_env.log_warn(
                        f"Hit max retries ({max_retries}), waiting before reconnecting"
                    )
                    await asyncio.sleep(10)  # Longer delay after max retries
                    retry_count = 0

                # Log connection attempt
                ten_env.log_info("Attempting to connect to Gemini...")

                # Connect and run the session
                await self._run_session(ten_env)

                # If _run_session exits normally, reset retry count
                retry_count = 0

            except Exception as e:
                retry_count += 1
                retry_delay = min(
                    60, base_retry_delay * (2**retry_count)
                )  # Exponential backoff

                traceback.print_exc()
                ten_env.log_error(
                    f"Connection error: {e}, retrying in {retry_delay} seconds"
                )

                # Close existing session if needed
                if self.session:
                    try:
                        await self.session.close()
                    except:
                        pass
                    self.session = None

                # Wait before retrying
                await asyncio.sleep(retry_delay)

                # Reset connection state
                self.connected = False

    async def _run_session(self, ten_env: AsyncTenEnv) -> None:
        """Run a session with optimized task handling."""
        connect_start_time = time.time()

        try:
            # Ensure client is initialized
            if not self.client:
                ten_env.log_error("Client not initialized, cannot connect")
                return

            # Get session configuration
            config = self._get_session_config()

            ten_env.log_info("Starting connection to Gemini service...")

            # Connect to session
            async with self.client.aio.live.connect(
                model=self.config.model, config=config
            ) as session:
                # Record connection time and setup session
                self.connect_times.append(time.time() - connect_start_time)
                session = cast(AsyncSession, session)
                self.session = session
                self.connected = True
                ten_env.log_info("Connected successfully")

                # Send greeting if needed
                if self.users_count > 0:
                    await self._greeting()

                # Create tasks for receiving responses
                response_task = asyncio.create_task(
                    self._receive_responses(ten_env)
                )

                # Wait until the session is stopped
                try:
                    while not self.stopped:
                        await asyncio.sleep(0.5)

                except asyncio.CancelledError:
                    ten_env.log_info("Session cancelled")

                finally:
                    # Cleanup response task
                    if not response_task.done():
                        response_task.cancel()

                    # Wait for task to complete
                    try:
                        await asyncio.wait_for(response_task, timeout=2.0)
                    except (asyncio.TimeoutError, asyncio.CancelledError):
                        pass

        except Exception as e:
            ten_env.log_error(f"Failed to establish connection: {e}")
            traceback.print_exc()

        finally:
            # Reset connection state
            self.connected = False
            self.session = None
            ten_env.log_info("Session ended")

    async def _receive_responses(self, ten_env: AsyncTenEnv) -> None:
        """Handle incoming responses from the model."""
        try:
            while self.connected and self.session and not self.stopped:
                try:
                    # Use receive() to get server messages
                    async for response in self.session.receive():
                        if self.stopped:
                            break

                        # Cast to correct type
                        response = cast(LiveServerMessage, response)

                        # Process server content
                        if response.server_content:
                            # Handle interruption
                            if response.server_content.interrupted:
                                ten_env.log_info("Interrupted")
                                await self._flush()
                                continue

                            # Process audio output (high priority)
                            if (
                                not response.server_content.turn_complete
                                and response.server_content.model_turn
                                and response.server_content.model_turn.parts
                            ):

                                # Create tasks for each audio part
                                for (
                                    part
                                ) in response.server_content.model_turn.parts:
                                    if (
                                        part.inline_data
                                        and part.inline_data.data
                                    ):
                                        # Create task for audio with high priority
                                        asyncio.create_task(
                                            self.send_audio_out(
                                                ten_env,
                                                part.inline_data.data,
                                                sample_rate=24000,
                                                bytes_per_sample=2,
                                                number_of_channels=1,
                                            )
                                        )

                            # Process transcriptions with lower priority
                            self._handle_transcriptions(
                                ten_env, response.server_content
                            )

                            # Handle turn completion
                            if response.server_content.turn_complete:
                                ten_env.log_info("Turn complete")

                        # Handle setup complete
                        elif response.setup_complete:
                            ten_env.log_info("Setup complete")

                        # Handle tool calls
                        elif (
                            response.tool_call
                            and response.tool_call.function_calls
                        ):
                            # Create task for tool handling
                            asyncio.create_task(
                                self._handle_tool_call(
                                    response.tool_call.function_calls
                                )
                            )

                except websockets.exceptions.ConnectionClosedOK:
                    ten_env.log_info("Connection closed normally")
                    break
                except websockets.exceptions.ConnectionClosedError as e:
                    ten_env.log_warn(f"Connection closed with error: {e}")
                    break
                except Exception as e:
                    ten_env.log_error(f"Error processing message: {e}")
                    continue

        except asyncio.CancelledError:
            ten_env.log_info("Response receiver cancelled")
        except Exception as e:
            ten_env.log_error(f"Error in response receiver: {e}")
            traceback.print_exc()

    def _handle_transcriptions(
        self, ten_env: AsyncTenEnv, server_content: LiveServerContent
    ) -> None:
        """Handle transcription responses with lower priority."""
        # Process input transcription
        if (
            server_content.input_transcription
            and server_content.input_transcription.text
        ):
            # Create task with lower priority
            asyncio.create_task(
                self._send_transcript(
                    server_content.input_transcription.text,
                    Role.User,
                    is_final=server_content.turn_complete or False,
                    end_of_segment=True,
                )
            )

        # Process output transcription
        if (
            server_content.output_transcription
            and server_content.output_transcription.text
        ):
            # Create task with lower priority
            asyncio.create_task(
                self._send_transcript(
                    server_content.output_transcription.text,
                    Role.Assistant,
                    is_final=server_content.turn_complete or False,
                    end_of_segment=True,
                )
            )

    async def send_audio_out(
        self, ten_env: AsyncTenEnv, audio_data: bytes, **args: TTSPcmOptions
    ) -> None:
        """End sending audio out."""
        sample_rate = args.get("sample_rate", 24000)
        bytes_per_sample = args.get("bytes_per_sample", 2)
        number_of_channels = args.get("number_of_channels", 1)
        try:
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
                await ten_env.send_audio_frame(f)
        except Exception:
            pass
            # ten_env.log_error(f"error send audio frame, {traceback.format_exc()}")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        ten_env.log_info("on_stop")

        self.stopped = True

        # Cancel all running tasks
        for task in self.tasks:
            if not task.done():
                task.cancel()

        # Clean up session
        if self.session:
            try:
                await self.session.close()
            except:
                pass

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        await super().on_audio_frame(ten_env, audio_frame)
        try:
            stream_id = audio_frame.get_property_int("stream_id")
            if self.channel_name == "":
                self.channel_name = audio_frame.get_property_string("channel")

            if self.remote_stream_id == 0:
                self.remote_stream_id = stream_id

            frame_buf = audio_frame.get_buf()
            self._dump_audio_if_need(frame_buf, Role.User)

            await self._on_audio(frame_buf)
            if not self.config.server_vad:
                self.input_end = time.time()
        except Exception as e:
            traceback.print_exc()
            self.ten_env.log_error(f"on audio frame failed {e}")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"on_cmd name {cmd_name}")

        status = StatusCode.OK
        detail = "success"

        if cmd_name == CMD_IN_FLUSH:
            # Will only flush if it is client side vad
            await self._flush()
            await ten_env.send_cmd(Cmd.create(CMD_OUT_FLUSH))
            ten_env.log_info("on flush")
        elif cmd_name == CMD_IN_ON_USER_JOINED:
            self.users_count += 1
            # Send greeting when first user joined
            if self.users_count == 1:
                await self._greeting()
        elif cmd_name == CMD_IN_ON_USER_LEFT:
            self.users_count -= 1
        else:
            # Register tool
            await super().on_cmd(ten_env, cmd)
            return

        cmd_result = CmdResult.create(status)
        cmd_result.set_property_string("detail", detail)
        await ten_env.return_result(cmd_result, cmd)

    # Not support for now
    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        pass

    async def on_video_frame(self, async_ten_env, video_frame):
        await super().on_video_frame(async_ten_env, video_frame)
        image_data = video_frame.get_buf()
        image_width = video_frame.get_width()
        image_height = video_frame.get_height()

        # Use non-blocking put to avoid memory buildup
        try:
            self.image_queue.put_nowait([image_data, image_width, image_height])
        except asyncio.QueueFull:
            # Drop frames if queue is full to maintain performance
            pass

    async def _on_video(self, _: AsyncTenEnv):
        while True:

            # Process the first frame from the queue
            [image_data, image_width, image_height] = (
                await self.image_queue.get()
            )
            self.video_buff = rgb2base64jpeg(
                image_data, image_width, image_height
            )
            # media_chunks = [
            #     {
            #         "data": self.video_buff,
            #         "mime_type": "image/jpeg",
            #     }
            # ]
            msg = {
                "data": self.video_buff,
                "mime_type": "image/jpeg",
            }
            try:
                if self.connected:
                    # ten_env.log_info(f"send image")
                    await self.session.send_realtime_input(
                        video=msg,
                    )
            except Exception as e:
                self.ten_env.log_error(f"Failed to send image {e}")

            # Skip remaining frames for the second
            while not self.image_queue.empty():
                await self.image_queue.get()

            # Wait for 1 second before processing the next frame
            await asyncio.sleep(1)

    # Direction: IN
    async def _on_audio(self, buff: bytearray):
        """Queue audio input for processing without blocking."""
        # Add audio to buffer
        self.buff += buff
        self.last_audio_time = time.time()  # Update last audio time

        # If we have enough audio data, queue it for processing
        if len(self.buff) >= self.audio_len_threshold:
            # Create a copy of the current buffer and clear it immediately
            current_buff = self.buff
            self.buff = bytearray()

            # Put the audio in the queue for processing
            try:
                self.audio_queue.put_nowait(current_buff)
            except asyncio.QueueFull:
                # If queue is full, drop oldest item and add new one
                try:
                    self.audio_queue.get_nowait()
                    self.audio_queue.put_nowait(current_buff)
                except:
                    pass

    async def _flush_audio_buffer(self):
        """Flush remaining audio in buffer if timeout reached."""
        if len(self.buff) > 0:
            current_buff = self.buff
            self.buff = bytearray()
            try:
                self.audio_queue.put_nowait(current_buff)
            except asyncio.QueueFull:
                try:
                    self.audio_queue.get_nowait()
                    self.audio_queue.put_nowait(current_buff)
                except:
                    pass

    async def _process_audio_queue(self, ten_env: AsyncTenEnv):
        """Process queued audio data in background."""
        while not self.stopped:
            try:
                # Check for timeout-based buffer flush (every 100ms)
                current_time = time.time()
                if (
                    current_time - self.last_audio_time > 0.5  # 500ms timeout
                    and len(self.buff) > 0
                ):
                    await self._flush_audio_buffer()
                    ten_env.log_debug("Flushed audio buffer due to timeout")

                # Wait for audio data from the queue
                if self.audio_queue.empty():
                    await asyncio.sleep(0.01)
                    continue

                # Get the audio data
                current_buff = await self.audio_queue.get()

                # Only process if we're connected
                if self.connected and self.session:
                    try:
                        # Convert to audio blob using asyncio.to_thread to avoid blocking
                        media_chunks = await asyncio.to_thread(
                            lambda: types.Blob(
                                data=current_buff,
                                mime_type="audio/pcm;rate=16000",
                            )
                        )

                        # Send audio data
                        await self.session.send_realtime_input(
                            audio=media_chunks
                        )
                    except Exception as e:
                        ten_env.log_error(f"Failed to send audio: {e}")
                else:
                    # Log when audio is dropped due to connection issues
                    ten_env.log_warn("Dropping audio: not connected to session")
            except asyncio.CancelledError:
                break
            except Exception as e:
                ten_env.log_error(f"Error processing audio queue: {e}")
                await asyncio.sleep(0.1)

    def _get_realtime_input_config(self) -> RealtimeInputConfig:
        """Extract and return configured speech sensitivities."""
        start_of_speech_sensitivity = (
            StartSensitivity.START_SENSITIVITY_UNSPECIFIED
        )
        end_of_speech_sensitivity = EndSensitivity.END_SENSITIVITY_UNSPECIFIED

        # Configure start of speech sensitivity
        if (
            (
                isinstance(self.config.start_of_speech_sensitivity, str)
                and self.config.start_of_speech_sensitivity.lower() == "high"
            )
            or self.config.start_of_speech_sensitivity
            == StartSensitivity.START_SENSITIVITY_HIGH
        ):
            start_of_speech_sensitivity = (
                StartSensitivity.START_SENSITIVITY_HIGH
            )
        elif (
            (
                isinstance(self.config.start_of_speech_sensitivity, str)
                and self.config.start_of_speech_sensitivity.lower() == "low"
            )
            or self.config.start_of_speech_sensitivity
            == StartSensitivity.START_SENSITIVITY_LOW
        ):
            start_of_speech_sensitivity = StartSensitivity.START_SENSITIVITY_LOW

        # Configure end of speech sensitivity
        if (
            (
                isinstance(self.config.end_of_speech_sensitivity, str)
                and self.config.end_of_speech_sensitivity.lower() == "high"
            )
            or self.config.end_of_speech_sensitivity
            == EndSensitivity.END_SENSITIVITY_HIGH
        ):
            end_of_speech_sensitivity = EndSensitivity.END_SENSITIVITY_HIGH
        elif (
            (
                isinstance(self.config.end_of_speech_sensitivity, str)
                and self.config.end_of_speech_sensitivity.lower() == "low"
            )
            or self.config.end_of_speech_sensitivity
            == EndSensitivity.END_SENSITIVITY_LOW
        ):
            end_of_speech_sensitivity = EndSensitivity.END_SENSITIVITY_LOW

        return RealtimeInputConfig(
            automatic_activity_detection=AutomaticActivityDetection(
                disabled=not self.config.server_vad,
                start_of_speech_sensitivity=start_of_speech_sensitivity,
                end_of_speech_sensitivity=end_of_speech_sensitivity,
                prefix_padding_ms=self.config.prefix_padding_ms,
                silence_duration_ms=self.config.silence_duration_ms,
            ),
        )

    def _get_session_config(self) -> LiveConnectConfigDict:
        # Return cached config if available to reduce cold start time
        if self._cached_session_config is not None:
            return self._cached_session_config

        def tool_dict(tool: LLMToolMetadata):
            required = []
            properties: dict[str, "Schema"] = {}

            for param in tool.parameters:
                properties[param.name] = Schema(
                    type=param.type.upper(), description=param.description
                )
                if param.required:
                    required.append(param.name)

            t = Tool(
                function_declarations=[
                    FunctionDeclaration(
                        name=tool.name,
                        description=tool.description,
                        parameters=Schema(
                            type="OBJECT",
                            properties=properties,
                            required=required,
                        ),
                    )
                ]
            )

            return t

        tools = (
            [tool_dict(t) for t in self.available_tools]
            if len(self.available_tools) > 0
            else []
        )

        tools.append(Tool(google_search={}))
        tools.append(Tool(code_execution={}))

        config = LiveConnectConfig(
            response_modalities=[Modality.AUDIO],
            # Add media resolution for optimized video processing performance
            media_resolution=self.config.media_resolution,
            system_instruction=Content(parts=[Part(text=self.config.prompt)]),
            tools=tools,
            # voice is currently not working
            speech_config=SpeechConfig(
                voice_config=VoiceConfig(
                    prebuilt_voice_config=PrebuiltVoiceConfig(
                        voice_name=self.config.voice
                    )
                ),
                language_code=self.config.language,
            ),
            generation_config=GenerationConfig(
                temperature=self.config.temperature,
                max_output_tokens=self.config.max_tokens,
            ),
            # Add context window compression for better performance with long conversations
            context_window_compression=types.ContextWindowCompressionConfig(
                trigger_tokens=self.config.context_window_trigger_tokens,
                sliding_window=types.SlidingWindow(
                    target_tokens=self.config.context_window_sliding_window_target_tokens
                ),
            ),
            realtime_input_config=self._get_realtime_input_config(),
            output_audio_transcription=(
                AudioTranscriptionConfig()
                if self.config and self.config.transcribe_agent
                else None
            ),
            input_audio_transcription=(
                AudioTranscriptionConfig()
                if self.config and self.config.transcribe_user
                else None
            ),
            enable_affective_dialog=(
                True if self.config.affective_dialog else None
            ),
            proactivity=(
                ProactivityConfig(proactive_audio=True)
                if self.config.proactive_audio
                else None
            ),
        )

        # Cache the configuration for future use
        self._cached_session_config = config
        return config

    async def on_tools_update(
        self, ten_env: AsyncTenEnv, tool: LLMToolMetadata
    ) -> None:
        """Called when a new tool is registered. Implement this method to process the new tool."""
        ten_env.log_info(f"on tools update {tool}")
        # await self._update_session()

    def _replace(self, prompt: str) -> str:
        result = prompt
        for token, value in self.ctx.items():
            result = result.replace("{" + token + "}", value)
        return result

    async def _send_transcript(
        self, content: str, role: Role, is_final: bool, end_of_segment: bool
    ) -> None:
        def is_punctuation(char):
            if char in [",", "，", ".", "。", "?", "？", "!", "！"]:
                return True
            return False

        def parse_sentences(sentence_fragment, content):
            sentences = []
            current_sentence = sentence_fragment
            for char in content:
                current_sentence += char
                if is_punctuation(char):
                    # Check if the current sentence contains non-punctuation characters
                    stripped_sentence = current_sentence
                    if any(c.isalnum() for c in stripped_sentence):
                        sentences.append(stripped_sentence)
                    current_sentence = ""  # Reset for the next sentence

            remain = current_sentence  # Any remaining characters form the incomplete sentence
            return sentences, remain

        def send_data(
            ten_env: AsyncTenEnv,
            sentence: str,
            stream_id: int,
            role: str,
            is_final: bool,
        ):
            try:
                d = Data.create("text_data")
                d.set_property_string("text", sentence)
                d.set_property_bool("end_of_segment", end_of_segment)
                d.set_property_string("role", role)
                d.set_property_int("stream_id", stream_id)
                d.set_property_bool("is_final", is_final)
                if is_final:
                    ten_env.log_info(
                        f"send transcript text [{sentence}] stream_id {stream_id} is_final {is_final} end_of_segment {is_final} role {role}"
                    )
                else:
                    ten_env.log_debug(
                        f"send transcript text [{sentence}] stream_id {stream_id} is_final {is_final} end_of_segment {is_final} role {role}"
                    )
                asyncio.create_task(ten_env.send_data(d))
            except Exception as e:
                ten_env.log_error(
                    f"Error send text data {role}: {sentence} {is_final} {e}"
                )

        stream_id = self.remote_stream_id if role == Role.User else 0
        try:
            if role == Role.Assistant and not is_final:
                sentences, self.transcript = parse_sentences(
                    self.transcript, content
                )
                for s in sentences:
                    asyncio.create_task(
                        send_data(self.ten_env, s, stream_id, role, is_final)
                    )
            else:
                asyncio.create_task(
                    send_data(self.ten_env, content, stream_id, role, is_final)
                )
        except Exception as e:
            self.ten_env.log_error(
                f"Error send text data {role}: {content} {is_final} {e}"
            )

    def _dump_audio_if_need(self, buf: bytearray, role: Role) -> None:
        if not self.config.dump:
            return

        with open(
            "{}_{}.pcm".format(role, self.channel_name), "ab"
        ) as dump_file:
            dump_file.write(buf)

    async def _handle_tool_call(self, func_calls: list[FunctionCall]) -> None:
        function_responses = []
        for call in func_calls:
            tool_call_id = call.id
            name = call.name
            arguments = call.args
            self.ten_env.log_info(
                f"_handle_tool_call {tool_call_id} {name} {arguments}"
            )
            cmd: Cmd = Cmd.create(CMD_TOOL_CALL)
            cmd.set_property_string("name", name)
            cmd.set_property_from_json("arguments", json.dumps(arguments))
            [result, _] = await self.ten_env.send_cmd(cmd)

            func_response = FunctionResponse(
                id=tool_call_id,
                name=name,
                response={"error": "Failed to call tool"},
            )
            if result.get_status_code() == StatusCode.OK:
                tool_result: LLMToolResult = json.loads(
                    result.get_property_to_json(CMD_PROPERTY_RESULT)
                )

                result_content = tool_result["content"]
                func_response = FunctionResponse(
                    id=tool_call_id,
                    name=name,
                    response={"output": result_content},
                )
                self.ten_env.log_info(
                    f"tool_result: {tool_call_id} {tool_result}"
                )
            else:
                self.ten_env.log_error("Tool call failed")
            function_responses.append(func_response)
            # await self.conn.send_request(tool_response)
            # await self.conn.send_request(ResponseCreate())
            self.ten_env.log_info(
                f"_remote_tool_call finish {name} {arguments}"
            )
        try:
            self.ten_env.log_info(f"send tool response {function_responses}")
            await self.session.send(
                LiveClientToolResponse(function_responses=function_responses)
            )
        except Exception as e:
            self.ten_env.log_error(f"Failed to send tool response {e}")

    def _greeting_text(self) -> str:
        text = "Hi, there."
        if self.config.language == "zh-CN":
            text = "你好。"
        elif self.config.language == "ja-JP":
            text = "こんにちは"
        elif self.config.language == "ko-KR":
            text = "안녕하세요"
        return text

    def _convert_tool_params_to_dict(self, tool: LLMToolMetadata):
        json_dict = {"type": "object", "properties": {}, "required": []}

        for param in tool.parameters:
            json_dict["properties"][param.name] = {
                "type": param.type,
                "description": param.description,
            }
            if param.required:
                json_dict["required"].append(param.name)

        return json_dict

    def _convert_to_content_parts(
        self, content: Iterable[LLMChatCompletionContentPartParam]
    ):
        content_parts = []

        if isinstance(content, str):
            content_parts.append({"type": "text", "text": content})
        else:
            for part in content:
                # Only text content is supported currently for v2v model
                if part["type"] == "text":
                    content_parts.append(part)
        return content_parts

    async def _greeting(self) -> None:
        if self.connected and self.users_count == 1:
            text = self._greeting_text()
            if self.config.greeting:
                text = "Say '" + self.config.greeting + "' to me."
            self.ten_env.log_info(f"send greeting {text}")
            await self.session.send_client_content(
                turns=Content(role=Role.User, parts=[Part(text=text)])
            )

    async def _flush(self) -> None:
        try:
            c = Cmd.create("flush")
            await self.ten_env.send_cmd(c)
        except Exception:
            self.ten_env.log_error("Error flush")

    async def _update_usage(self, usage: dict) -> None:
        self.total_usage.completion_tokens += usage.get("output_tokens")
        self.total_usage.prompt_tokens += usage.get("input_tokens")
        self.total_usage.total_tokens += usage.get("total_tokens")
        if not self.total_usage.completion_tokens_details:
            self.total_usage.completion_tokens_details = (
                LLMCompletionTokensDetails()
            )
        if not self.total_usage.prompt_tokens_details:
            self.total_usage.prompt_tokens_details = LLMPromptTokensDetails()

        if usage.get("output_token_details"):
            self.total_usage.completion_tokens_details.accepted_prediction_tokens += usage[
                "output_token_details"
            ].get(
                "text_tokens"
            )
            self.total_usage.completion_tokens_details.audio_tokens += usage[
                "output_token_details"
            ].get("audio_tokens")

        if usage.get("input_token_details:"):
            self.total_usage.prompt_tokens_details.audio_tokens += usage[
                "input_token_details"
            ].get("audio_tokens")
            self.total_usage.prompt_tokens_details.cached_tokens += usage[
                "input_token_details"
            ].get("cached_tokens")
            self.total_usage.prompt_tokens_details.text_tokens += usage[
                "input_token_details"
            ].get("text_tokens")

        self.ten_env.log_info(f"total usage: {self.total_usage}")

        data = Data.create("llm_stat")
        data.set_property_from_json(
            "usage", json.dumps(self.total_usage.model_dump())
        )
        if (
            self.connect_times
            and self.completion_times
            and self.first_token_times
        ):
            data.set_property_from_json(
                "latency",
                json.dumps(
                    {
                        "connection_latency_95": np.percentile(
                            self.connect_times, 95
                        ),
                        "completion_latency_95": np.percentile(
                            self.completion_times, 95
                        ),
                        "first_token_latency_95": np.percentile(
                            self.first_token_times, 95
                        ),
                        "connection_latency_99": np.percentile(
                            self.connect_times, 99
                        ),
                        "completion_latency_99": np.percentile(
                            self.completion_times, 99
                        ),
                        "first_token_latency_99": np.percentile(
                            self.first_token_times, 99
                        ),
                    }
                ),
            )
        asyncio.create_task(self.ten_env.send_data(data))

    async def on_call_chat_completion(self, async_ten_env, **kargs):
        raise NotImplementedError

    async def on_data_chat_completion(self, async_ten_env, **kargs):
        raise NotImplementedError
