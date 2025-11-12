#
# Agora Real Time Engagement
# StepFun Realtime MLLM — aligned to OpenAIRealtime2Extension
# Created by Wei Hu in 2024-08. Refactor by <you>.
#
import asyncio
import base64
import traceback
import time
from dataclasses import dataclass
from typing import Literal

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
from ten_runtime import AudioFrame, AsyncTenEnv

from ten_ai_base.types import LLMToolMetadata

from .realtime.connection import RealtimeApiConnection
from .realtime.struct import (
    AssistantMessageItemParam,
    ItemCreate,
    ItemInputAudioTranscriptionDelta,
    SessionCreated,
    ItemCreated,
    SessionUpdated,
    UserMessageItemParam,
    ItemInputAudioTranscriptionCompleted,
    ItemInputAudioTranscriptionFailed,
    ResponseCreated,
    ResponseDone,
    ResponseAudioTranscriptDelta,
    ResponseTextDelta,
    ResponseAudioTranscriptDone,
    ResponseTextDone,
    ResponseOutputItemDone,
    ResponseOutputItemAdded,
    ResponseAudioDelta,
    ResponseAudioDone,
    InputAudioBufferSpeechStarted,
    InputAudioBufferSpeechStopped,
    ResponseFunctionCallArgumentsDone,
    ErrorMessage,
    SessionUpdate,
    SessionUpdateParams,
    InputAudioTranscription,
    ContentType,
    FunctionCallOutputItemParam,
    ResponseCreate,
    ServerVADUpdateParams,
)


@dataclass
class StepFunRealtimeConfig(BaseModel):
    base_url: str = "wss://api.stepfun.com"
    api_key: str = ""
    path: str = "/v1/realtime"
    model: str = "step-1o-audio"
    language: str = "en"
    prompt: str = ""
    temperature: float = 0.5
    max_tokens: int = 1024
    voice: str = "linjiajiejie"
    server_vad: bool = True
    audio_out: bool = True
    sample_rate: int = 24000

    # VAD tuning
    vad_type: Literal["server_vad", "semantic_vad"] = "server_vad"
    vad_eagerness: Literal["low", "medium", "high", "auto"] = "auto"
    vad_threshold: float = 0.5
    vad_prefix_padding_ms: int = 300
    vad_silence_duration_ms: int = 500

    dump: bool = False
    dump_path: str = ""


class StepFunRealtime2Extension(AsyncMLLMBaseExtension):
    """
    StepFun realtime provider, API-compatible with OpenAIRealtime2Extension:
    - same public methods
    - same server event mapping -> send_server_* APIs
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.conn: RealtimeApiConnection | None = None
        self.session = None
        self.session_id: str | None = None

        self.config: StepFunRealtimeConfig | None = None
        self.stopped: bool = False
        self.connected: bool = False

        self.request_transcript: str = ""
        self.response_transcript: str = ""
        self.available_tools: list[LLMToolMetadata] = []

        self.loop: asyncio.AbstractEventLoop | None = None

    # ---------- Lifecycle ----------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")
        self.ten_env = ten_env
        self.loop = asyncio.get_event_loop()

        properties, _ = await ten_env.get_property_to_json(None)
        self.config = StepFunRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if not self.config.api_key:
            ten_env.log_error("api_key is required")
            raise ValueError("api_key is required")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_stop")
        await super().on_stop(ten_env)
        self.stopped = True
        if self.conn:
            await self.conn.close()

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def vendor(self) -> str:
        return "stepfun"

    async def start_connection(self) -> None:
        try:
            self.conn = RealtimeApiConnection(
                ten_env=self.ten_env,
                base_url=self.config.base_url,
                path=self.config.path,
                api_key=self.config.api_key,
                model=self.config.model,
            )

            await self.conn.connect()
            item_id = ""  # For truncate tracking
            response_id = ""
            flushed: set[str] = set()
            session_start_ms = int(time.time() * 1000)

            self.ten_env.log_info("Client loop started")
            async for message in self.conn.listen():
                try:
                    match message:
                        # ----- session lifecycle -----
                        case SessionCreated():
                            self.ten_env.log_info(
                                f"Session created: {message.session}"
                            )
                            self.connected = True
                            self.session_id = message.session.id
                            self.session = message.session
                            await self._update_session()
                            await self._resume_context(self.message_context)
                        case SessionUpdated():
                            self.ten_env.log_info(
                                f"Session updated: {message.session}"
                            )
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )

                        # ----- input speech transcription (user) -----
                        case ItemInputAudioTranscriptionDelta():
                            self.ten_env.log_debug(
                                f"Req transcript delta {message.item_id} {message.content_index}"
                            )
                            self.request_transcript += message.delta
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=self.request_transcript,
                                    delta=message.delta,
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                        case ItemInputAudioTranscriptionCompleted():
                            self.ten_env.log_debug(
                                f"Req transcript done {message.transcript}"
                            )
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=self.request_transcript,
                                    delta=message.transcript,
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.request_transcript = ""
                        case ItemInputAudioTranscriptionFailed():
                            self.ten_env.log_warn(
                                f"Req transcript failed {message.item_id} {message.error}"
                            )
                            self.request_transcript = ""

                        # ----- output content events (assistant) -----
                        case ItemCreated():
                            self.ten_env.log_debug(
                                f"Item created {message.item}"
                            )
                        case ResponseCreated():
                            response_id = message.response.id
                            self.ten_env.log_debug(
                                f"Resp created {response_id}"
                            )
                        case ResponseDone():
                            rid = message.response.id
                            status = message.response.status
                            if rid == response_id:
                                response_id = ""
                            self.ten_env.log_debug(
                                f"Resp done {rid} {status} {message.response.usage}"
                            )
                            # optionally update usage

                        # text stream
                        case ResponseTextDelta():
                            self.ten_env.log_debug(
                                f"Resp text delta {message.response_id} {message.output_index} {message.content_index} {message.delta}"
                            )
                            if message.response_id in flushed:
                                self.ten_env.log_warn(
                                    f"Ignored flushed text delta {message.response_id}"
                                )
                                continue
                            if item_id != message.item_id:
                                item_id = message.item_id
                            self.response_transcript += message.delta
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta=message.delta,
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                        case ResponseTextDone():
                            self.ten_env.log_debug(
                                f"Resp text done {message.output_index} {message.content_index} {message.text}"
                            )
                            if message.response_id in flushed:
                                self.ten_env.log_warn(
                                    f"Ignored flushed text done {message.response_id}"
                                )
                                continue
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta="",
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.response_transcript = ""

                        # audio transcript stream (assistant)
                        case ResponseAudioTranscriptDelta():
                            self.ten_env.log_debug(
                                f"Resp transcript delta {message.response_id} {message.output_index} {message.content_index} {message.delta}"
                            )
                            if message.response_id in flushed:
                                self.ten_env.log_warn(
                                    f"Ignored flushed transcript delta {message.response_id}"
                                )
                                continue
                            self.response_transcript += message.delta
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta=message.delta,
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                        case ResponseAudioTranscriptDone():
                            self.ten_env.log_debug(
                                f"Resp transcript done {message.output_index} {message.content_index} {message.transcript}"
                            )
                            if message.response_id in flushed:
                                self.ten_env.log_warn(
                                    f"Ignored flushed transcript done {message.response_id}"
                                )
                                continue
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta="",
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.response_transcript = ""

                        # raw audio PCM delta from model
                        case ResponseAudioDelta():
                            if message.response_id in flushed:
                                self.ten_env.log_warn(
                                    f"Ignored flushed audio delta {message.response_id}"
                                )
                                continue
                            if item_id != message.item_id:
                                item_id = message.item_id
                            audio_data = base64.b64decode(message.delta)
                            await self.send_server_output_audio_data(audio_data)
                        case ResponseAudioDone():
                            pass

                        # VAD / turn-taking
                        case InputAudioBufferSpeechStarted():
                            self.ten_env.log_info(
                                f"Server listening, in response {response_id}, last item {item_id}"
                            )
                            # compute relative end time (ms) since session start
                            # current_ms = int(time.time() * 1000)
                            # end_ms = current_ms - session_start_ms
                            # (optional) truncate on-going generation by item_id/content_index if supported
                            if self.config.server_vad:
                                await self.send_server_interrupted(
                                    sos=MLLMServerInterrupt()
                                )
                            if response_id and self.response_transcript:
                                transcript = (
                                    self.response_transcript + "[interrupted]"
                                )
                                await self.send_server_output_text(
                                    MLLMServerOutputTranscript(
                                        content=transcript,
                                        delta=None,
                                        final=True,
                                        metadata={
                                            "session_id": self.session_id
                                            or "-1"
                                        },
                                    )
                                )
                                self.response_transcript = ""
                                flushed.add(response_id)
                            item_id = ""
                        case InputAudioBufferSpeechStopped():
                            # only meaningful when server_vad is on
                            # shift session_start_ms to keep relative timing aligned with provider
                            session_start_ms = (
                                int(time.time() * 1000) - message.audio_end_ms
                            )
                            self.ten_env.log_info(
                                f"Server stop listening, audio_end_ms={message.audio_end_ms}, session_start_ms={session_start_ms}"
                            )

                        # tools
                        case ResponseFunctionCallArgumentsDone():
                            tool_call_id = message.call_id
                            name = message.name
                            arguments = message.arguments
                            self.ten_env.log_info(f"need to call func {name}")
                            self.loop.create_task(
                                self._handle_tool_call(
                                    tool_call_id, name, arguments
                                )
                            )

                        # misc
                        case ResponseOutputItemDone():
                            self.ten_env.log_debug(
                                f"Output item done {message.item}"
                            )
                        case ResponseOutputItemAdded():
                            self.ten_env.log_debug(
                                f"Output item added {message.output_index} {message.item}"
                            )
                        case ErrorMessage():
                            self.ten_env.log_error(
                                f"Error message received: {message.error}"
                            )
                        case _:
                            self.ten_env.log_debug(
                                f"Not handled message {message}"
                            )

                except Exception as e:
                    traceback.print_exc()
                    self.ten_env.log_error(
                        f"Error processing message: {message} {e}"
                    )

            self.ten_env.log_info("Client loop finished")
        except Exception as e:
            traceback.print_exc()
            self.ten_env.log_error(f"Failed to handle loop {e}")

        await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.connected = False
        if self.conn:
            await self.conn.close()

    async def _handle_reconnect(self) -> None:
        """Handle reconnection logic with small backoff."""
        await self.stop_connection()
        if not self.stopped:
            await asyncio.sleep(1)
            await self.start_connection()

    def is_connected(self) -> bool:
        return self.connected

    # ---------- Client → Provider (ingress) ----------

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        await self.conn.send_audio_data(frame.get_buf())
        return True

    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        """
        Send a text message item to the model (user/assistant).
        """
        match item.role:
            case "user":
                await self.conn.send_request(
                    ItemCreate(
                        item=UserMessageItemParam(
                            content=[
                                {
                                    "type": ContentType.InputText,
                                    "text": item.content,
                                }
                            ]
                        )
                    )
                )
            case "assistant":
                await self.conn.send_request(
                    ItemCreate(
                        item=AssistantMessageItemParam(
                            content=[
                                {"type": ContentType.Text, "text": item.content}
                            ]
                        )
                    )
                )
            case _:
                self.ten_env.log_error(f"Unknown role: {item.role}")

    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        """Trigger the model to generate."""
        await self.conn.send_request(ResponseCreate())

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        """Register tool and update session."""
        self.available_tools.append(tool)
        await self._update_session()

    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        """Return tool result back to model."""
        self.ten_env.log_info(
            f"Sending function call output: {function_call_output.output}"
        )
        await self.conn.send_request(
            ItemCreate(
                item=FunctionCallOutputItemParam(
                    call_id=function_call_output.call_id,
                    output=function_call_output.output,
                )
            )
        )

    async def _resume_context(
        self, messages: list[MLLMClientMessageItem]
    ) -> None:
        """Replay preserved messages into current session."""
        for message in messages:
            self.ten_env.log_info(f"Resuming context with message: {message}")
            await self.send_client_message_item(message)

    # ---------- Session update / tools ----------

    async def _update_session(self) -> None:
        if not self.connected:
            self.ten_env.log_warn("Not connected to StepFun session")
            return

        tools = []

        def tool_dict(tool: LLMToolMetadata):
            t = {
                "type": "function",
                "name": tool.name,
                "description": tool.description,
                "parameters": {
                    "type": "object",
                    "properties": {},
                    "required": [],
                    "additionalProperties": False,
                },
            }
            for p in tool.parameters:
                t["parameters"]["properties"][p.name] = {
                    "type": p.type,
                    "description": p.description,
                }
                if p.required:
                    t["parameters"]["required"].append(p.name)
            return t

        if self.available_tools:
            tools = [tool_dict(t) for t in self.available_tools]

        # VAD params
        vad_params = None
        if self.config.vad_type == "server_vad":
            vad_params = ServerVADUpdateParams(
                threshold=self.config.vad_threshold,
                prefix_padding_ms=self.config.vad_prefix_padding_ms,
                silence_duration_ms=self.config.vad_silence_duration_ms,
            )

        su = SessionUpdate(
            session=SessionUpdateParams(
                instructions=self.config.prompt,
                model=self.config.model,
                tool_choice="auto" if self.available_tools else "none",
                tools=tools,
                turn_detection=vad_params,
            )
        )
        if self.config.audio_out:
            su.session.voice = self.config.voice
        else:
            su.session.modalities = ["text"]

        su.session.input_audio_transcription = InputAudioTranscription(
            language=self.config.language
        )

        self.ten_env.log_info(
            f"update session instructions={self.config.prompt} tools={len(tools)}"
        )
        await self.conn.send_request(su)

    # ---------- Tool call bridging ----------

    async def _handle_tool_call(
        self, tool_call_id: str, name: str, arguments: str
    ) -> None:
        self.ten_env.log_info(
            f"_handle_tool_call {tool_call_id} {name} {arguments}"
        )
        await self.send_server_function_call(
            MLLMServerFunctionCall(
                call_id=tool_call_id,
                name=name,
                arguments=arguments,
            )
        )
