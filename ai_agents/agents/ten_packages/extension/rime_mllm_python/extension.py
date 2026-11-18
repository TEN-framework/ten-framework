"""Translate TEN's standard MLLM audio and caption messages to Prism."""

import asyncio
import base64
import json

from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
from ten_ai_base.mllm import AsyncMLLMBaseExtension, DATA_MLLM_IN_SET_MESSAGE_CONTEXT
from ten_ai_base.struct import (
    MLLMServerInputTranscript,
    MLLMServerInterrupt,
    MLLMServerOutputTranscript,
    MLLMServerSessionReady,
)

from .audio import AudioBufferOverflow, InputAudio
from .config import (
    CHANNELS,
    INPUT_FRAME_SECONDS,
    INPUT_SAMPLE_RATE,
    OUTPUT_SAMPLE_RATE,
    SAMPLE_WIDTH,
    RimeRealtimeConfig,
)
from .connection import PrismConnection, PrismError


class RimeRealtimeExtension(AsyncMLLMBaseExtension):
    """Own a continuous input clock and forward only final captions."""

    def __init__(self, name):
        super().__init__(name)
        self.config = None
        self.connection = None
        self.input_audio = InputAudio()
        self._audio_sender = None
        self._pending_text = None

    async def on_init(self, ten_env):
        self.ten_env = ten_env
        properties, error = await ten_env.get_property_to_json(None)
        try:
            if error:
                raise ValueError("cannot read properties")
            self.config = RimeRealtimeConfig.model_validate_json(properties)
            ten_env.log_info(f"config: {self.config.model_dump_json()}")
            self.connection = PrismConnection(self.config, self._on_event)
        except ValueError:
            await self._report("invalid_config", "Invalid Rime configuration", True)

    async def on_start(self, _ten_env):
        if self.connection is not None:
            try:
                await self.start_connection()
            except Exception as error:
                await self._report(
                    getattr(error, "code", "startup_failed"),
                    "Prism session could not start",
                    True,
                )

    async def start_connection(self):
        await self.connection.start()
        if not self.is_connected():
            raise PrismError("connection_closed", "Prism ended during startup", True)
        self._audio_sender = asyncio.create_task(self._send_input())
        await self.send_server_session_ready(MLLMServerSessionReady())

    async def on_stop(self, _ten_env):
        self.stopped = True
        await self.stop_connection()

    async def stop_connection(self):
        self.input_audio.buffer.clear()
        if self._audio_sender is not None:
            if self._audio_sender is not asyncio.current_task():
                self._audio_sender.cancel()
                await asyncio.gather(self._audio_sender, return_exceptions=True)
            self._audio_sender = None
        if self.connection is not None:
            await self.connection.close()

    def is_connected(self):
        return (
            self.connection is not None and self.connection.ready and not self.stopped
        )

    def vendor(self):
        return "rime"

    def input_audio_sample_rate(self):
        return INPUT_SAMPLE_RATE

    def synthesize_audio_sample_rate(self):
        return OUTPUT_SAMPLE_RATE

    async def on_audio_frame(self, _ten_env, audio_frame):
        if not self.is_connected():
            return
        try:
            await self.send_audio(audio_frame, self.session_id)
        except AudioBufferOverflow:
            await self._report("audio_backlog", "Prism input fell behind", True)
        except ValueError:
            await self._report(
                "invalid_audio", "Expected live 16 kHz mono PCM16", False
            )

    async def send_audio(self, frame, session_id):
        if (
            frame.get_sample_rate() != INPUT_SAMPLE_RATE
            or frame.get_number_of_channels() != CHANNELS
            or frame.get_bytes_per_sample() != SAMPLE_WIDTH
        ):
            raise ValueError("incorrect audio format")
        metadata, _ = frame.get_property_to_json("metadata")
        if metadata:
            properties = json.loads(metadata)
            if not isinstance(properties, dict):
                raise ValueError("audio metadata must be an object")
            self.session_id = properties.get("session_id", session_id)
        self.input_audio.append(bytes(frame.get_buf()))
        return True

    async def _send_input(self):
        loop = asyncio.get_running_loop()
        deadline = loop.time()
        try:
            while self.is_connected():
                await self.connection.send_audio(self.input_audio.frame())
                # Preserve cadence through jitter; limit catch-up after a stall.
                deadline = max(
                    deadline + INPUT_FRAME_SECONDS, loop.time() - INPUT_FRAME_SECONDS
                )
                await asyncio.sleep(max(0, deadline - loop.time()))
        except (PrismError, OSError):
            await self._report("audio_send_failed", "Prism input stopped", True)

    async def on_data(self, ten_env, data):
        if self.connection is None or self.stopped:
            return
        try:
            if data.get_name() == DATA_MLLM_IN_SET_MESSAGE_CONTEXT:
                raise PrismError(
                    "unsupported_history", "History loading is not supported"
                )
            await super().on_data(ten_env, data)
        except (PrismError, ValueError) as error:
            await self._report(
                getattr(error, "code", "invalid_request"),
                "Rime request could not be applied",
                isinstance(error, PrismError) and error.fatal,
            )

    async def send_client_message_item(self, item, session_id=None):
        if item.role != "user":
            raise PrismError("unsupported_history", "Only user text is supported")
        if self._pending_text is not None:
            raise PrismError("pending_text", "Trigger the pending text first")
        self._pending_text = item.content

    async def send_client_create_response(self, session_id=None):
        await self.connection.trigger(self._pending_text)
        self._pending_text = None

    async def send_client_register_tool(self, tool):
        await self._report(
            "unsupported_tools", "This provider does not support tools", False
        )

    async def send_client_function_call_output(self, function_call_output):
        await self._report(
            "unsupported_tools", "This provider does not support tools", False
        )

    async def _on_event(self, event):
        kind = event["type"]
        if kind == "input_audio_buffer.speech_started":
            self.leftover_bytes = b""
            await self.send_server_interrupted(MLLMServerInterrupt())
        elif kind in {
            "conversation.item.input_audio_transcription.completed",
            "conversation.item.input_audio_transcription.failed",
        }:
            text = event.get("transcript", "")
            await self.send_server_input_transcript(
                MLLMServerInputTranscript(content=text, delta=text, final=True)
            )
            if kind.endswith("failed"):
                await self._report(
                    "transcription_failed", "Caller transcription failed", False
                )
        elif kind == "response.text.done":
            text = event["text"]
            if text:
                await self.send_server_output_text(
                    MLLMServerOutputTranscript(content=text, delta=text, final=True)
                )
        elif kind == "response.audio.delta":
            audio = base64.b64decode(event["delta"], validate=True)
            if len(audio) % SAMPLE_WIDTH:
                raise ValueError("partial output PCM16 sample")
            await self.send_server_output_audio_data(audio)
        elif kind == "error":
            error = event["error"]
            await self._report(
                error.get("code", "vendor_error"),
                "Prism reported an error",
                error.get("scope") == "session",
            )

    async def _report(self, code, message, fatal):
        if fatal:
            if self.stopped:
                return
            self.stopped = True
        logger = self.ten_env.log_error if fatal else self.ten_env.log_warn
        logger(f"Rime {code}: {message}")
        try:
            await self.send_mllm_error(
                ModuleError(
                    code=int(
                        ModuleErrorCode.FATAL_ERROR.value
                        if fatal
                        else ModuleErrorCode.NON_FATAL_ERROR.value
                    ),
                    message=message,
                ),
                ModuleErrorVendorInfo(vendor=self.vendor(), code=code, message=message),
            )
        finally:
            if fatal:
                await self.stop_connection()
