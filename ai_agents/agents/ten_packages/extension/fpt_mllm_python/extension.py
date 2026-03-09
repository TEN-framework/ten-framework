import asyncio
import base64
import time
import traceback

from pydantic import BaseModel

from ten_ai_base.mllm import AsyncMLLMBaseExtension
from ten_ai_base.struct import (
    MLLMClientFunctionCallOutput,
    MLLMClientMessageItem,
    MLLMServerFunctionCall,
    MLLMServerInputTranscript,
    MLLMServerInterrupt,
    MLLMServerOutputTranscript,
    MLLMServerSessionReady,
)
from ten_ai_base.types import LLMToolMetadata
from ten_runtime import AsyncTenEnv, AudioFrame, Data

from .realtime.connection import FPTTokenManager, RealtimeApiConnection
from .realtime.struct import (
    ErrorMessage,
    FunctionCallArgumentsDone,
    InputTranscriptCompleted,
    InputTranscriptDelta,
    InputTranscriptFailed,
    ItemCreate,
    ResponseAudioDelta,
    ResponseAudioDone,
    ResponseCreate,
    ResponseCreated,
    ResponseDone,
    ResponseTextDelta,
    ResponseTextDone,
    SessionCreated,
    SessionUpdate,
    SessionUpdated,
    SpeechStarted,
    SpeechStopped,
    UnknownMessage,
)


class FPTRealtimeConfig(BaseModel):
    token_url: str = ""
    websocket_url: str = ""
    api_key: str = ""
    app_id: str = ""
    model: str = "vi-gpt-realtime"
    language: str = "vi-VN"
    prompt: str = ""
    temperature: float = 0.8
    max_tokens: int = 1024
    voice: str = "default"
    server_vad: bool = True
    audio_out: bool = True
    input_transcript: bool = True
    sample_rate: int = 16000
    vendor: str = "fpt"
    dump: bool = False
    dump_path: str = ""


class FPTRealtimeExtension(AsyncMLLMBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.config: FPTRealtimeConfig | None = None
        self.conn: RealtimeApiConnection | None = None
        self.token_manager: FPTTokenManager | None = None
        self.connected = False
        self.request_transcript = ""
        self.response_transcript = ""
        self.available_tools: list[LLMToolMetadata] = []

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env
        properties, _ = await ten_env.get_property_to_json(None)
        self.config = FPTRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if not self.config.token_url:
            raise ValueError("token_url is required")
        if not self.config.websocket_url:
            raise ValueError("websocket_url is required")
        if not self.config.api_key:
            raise ValueError("api_key is required")

        self.token_manager = FPTTokenManager(
            ten_env=ten_env,
            token_url=self.config.token_url,
            api_key=self.config.api_key,
            app_id=self.config.app_id,
        )

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        if self.token_manager is not None:
            await self.token_manager.close()

    def vendor(self) -> str:
        return self.config.vendor

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def is_connected(self) -> bool:
        return self.connected

    async def start_connection(self) -> None:
        try:
            assert self.token_manager is not None
            token = await self.token_manager.get_token()
            self.conn = RealtimeApiConnection(
                ten_env=self.ten_env,
                websocket_url=self.config.websocket_url,
                token=token,
                verbose=self.config.dump,
            )
            await self.conn.connect()

            response_id = ""
            item_id = ""
            flushed: set[str] = set()
            self.ten_env.log_info("FPT realtime loop started")
            async for message in self.conn.listen():
                try:
                    match message:
                        case SessionCreated():
                            self.connected = True
                            self.ten_env.log_info(
                                f"FPT session created: {message.session_id}"
                            )
                            await self._update_session()
                            await self._resume_context(self.message_context)
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )
                        case SessionUpdated():
                            self.connected = True
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )
                        case InputTranscriptDelta():
                            self.request_transcript += message.delta
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=self.request_transcript,
                                    delta=message.delta,
                                    final=False,
                                    metadata={},
                                )
                            )
                        case InputTranscriptCompleted():
                            final_text = (
                                message.transcript or self.request_transcript
                            )
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=final_text,
                                    delta="",
                                    final=True,
                                    metadata={},
                                )
                            )
                            self.request_transcript = ""
                        case InputTranscriptFailed():
                            self.ten_env.log_warn(
                                f"FPT input transcript failed: {message.error}"
                            )
                            self.request_transcript = ""
                        case ResponseCreated():
                            response_id = message.response_id
                        case ResponseDone():
                            if message.response_id == response_id:
                                response_id = ""
                        case ResponseTextDelta():
                            if message.response_id in flushed:
                                continue
                            item_id = message.item_id or item_id
                            self.response_transcript += message.delta
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta=message.delta,
                                    final=False,
                                    metadata={},
                                )
                            )
                        case ResponseTextDone():
                            if message.response_id in flushed:
                                continue
                            final_text = (
                                self.response_transcript or message.text
                            )
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=final_text,
                                    delta="",
                                    final=True,
                                    metadata={},
                                )
                            )
                            self.response_transcript = ""
                        case ResponseAudioDelta():
                            if message.response_id in flushed:
                                continue
                            item_id = message.item_id or item_id
                            await self.send_server_output_audio_data(
                                base64.b64decode(message.delta)
                            )
                        case ResponseAudioDone():
                            self.ten_env.log_debug(
                                f"FPT audio done: {message.response_id}"
                            )
                        case SpeechStarted():
                            if self.config.server_vad:
                                await self.send_server_interrupted(
                                    MLLMServerInterrupt()
                                )
                            if response_id and self.response_transcript:
                                await self.send_server_output_text(
                                    MLLMServerOutputTranscript(
                                        content=(
                                            self.response_transcript
                                            + "[interrupted]"
                                        ),
                                        delta="",
                                        final=True,
                                        metadata={},
                                    )
                                )
                                flushed.add(response_id)
                                self.response_transcript = ""
                            item_id = ""
                        case SpeechStopped():
                            speech_stopped_at = (
                                int(time.time() * 1000)
                                - message.audio_end_ms
                            )
                            self.ten_env.log_debug(
                                f"FPT speech stopped at {speech_stopped_at}"
                            )
                        case FunctionCallArgumentsDone():
                            await self.send_server_function_call(
                                MLLMServerFunctionCall(
                                    call_id=message.call_id,
                                    name=message.name,
                                    arguments=message.arguments,
                                )
                            )
                        case ErrorMessage():
                            self.ten_env.log_error(
                                f"FPT realtime error: {message.error}"
                            )
                        case UnknownMessage():
                            self.ten_env.log_debug(
                                f"Unhandled FPT message: {message.raw}"
                            )
                        case _:
                            self.ten_env.log_debug(
                                f"Unknown FPT event object: {message}"
                            )
                except Exception as exc:
                    traceback.print_exc()
                    self.ten_env.log_error(
                        f"Failed to process FPT message {message}: {exc}"
                    )

            self.ten_env.log_info("FPT realtime loop finished")
        except Exception as exc:
            traceback.print_exc()
            self.ten_env.log_error(f"FPT start_connection failed: {exc}")
            if self._should_refresh_token(exc):
                await self._refresh_token()

        await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.connected = False
        if self.conn is not None:
            await self.conn.close()
            self.conn = None

    async def _handle_reconnect(self) -> None:
        await self.stop_connection()
        if not self.stopped:
            await asyncio.sleep(1)
            await self.start_connection()

    async def _refresh_token(self) -> None:
        if self.token_manager is None:
            return
        try:
            await self.token_manager.get_token(force_refresh=True)
        except Exception as exc:
            self.ten_env.log_error(f"FPT token refresh failed: {exc}")

    def _should_refresh_token(self, exc: Exception) -> bool:
        message = str(exc).lower()
        return "401" in message or "403" in message or "auth" in message

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        if self.conn is None:
            return False
        await self.conn.send_audio_data(frame.get_buf())
        return True

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        await super().on_data(ten_env, data)

    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        if self.conn is None:
            return

        message_item = {
            "type": "message",
            "role": item.role,
            "content": [
                {
                    "type": "input_text" if item.role == "user" else "text",
                    "text": item.content or "",
                }
            ],
        }
        await self.conn.send_request(ItemCreate(item=message_item))

    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        if self.conn is None:
            return
        await self.conn.send_request(ResponseCreate())

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        self.available_tools.append(tool)
        await self._update_session()

    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        if self.conn is None:
            return

        await self.conn.send_request(
            ItemCreate(
                item={
                    "type": "function_call_output",
                    "call_id": function_call_output.call_id,
                    "output": function_call_output.output,
                }
            )
        )
        await self.conn.send_request(ResponseCreate())

    async def _resume_context(
        self, messages: list[MLLMClientMessageItem]
    ) -> None:
        for message in messages:
            await self.send_client_message_item(message)

    async def _update_session(self) -> None:
        if not self.connected or self.conn is None:
            return

        tools: list[dict[str, object]] = []
        for tool in self.available_tools:
            properties: dict[str, object] = {}
            required: list[str] = []
            for param in tool.parameters:
                properties[param.name] = {
                    "type": param.type,
                    "description": param.description,
                }
                if param.required:
                    required.append(param.name)
            tools.append(
                {
                    "type": "function",
                    "name": tool.name,
                    "description": tool.description,
                    "parameters": {
                        "type": "object",
                        "properties": properties,
                        "required": required,
                        "additionalProperties": False,
                    },
                }
            )

        session: dict[str, object] = {
            "model": self.config.model,
            "instructions": self.config.prompt,
            "temperature": self.config.temperature,
            "max_response_output_tokens": self.config.max_tokens,
            "turn_detection": {
                "type": "server_vad" if self.config.server_vad else "none"
            },
            "input_audio_format": "pcm16",
            "output_audio_format": "pcm16",
            "tool_choice": "auto" if tools else "none",
            "tools": tools,
        }
        if self.config.audio_out:
            session["voice"] = self.config.voice
            session["modalities"] = ["text", "audio"]
        else:
            session["modalities"] = ["text"]
        if self.config.input_transcript:
            session["input_audio_transcription"] = {
                "language": self.config.language
            }

        await self.conn.send_request(SessionUpdate(session=session))
