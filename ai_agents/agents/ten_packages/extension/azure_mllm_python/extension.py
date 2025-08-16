#
# Agora Real Time Engagement
# Azure Realtime MLLM — aligned to OpenAIRealtime2Extension pattern
# Created by Wei Hu in 2024-08. Refactor by <you>.
#
import asyncio
import base64
import time
import traceback
from dataclasses import dataclass

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
from ten_runtime import AudioFrame, AsyncTenEnv, Data

from ten_ai_base.types import LLMToolMetadata

from .realtime.connection import RealtimeApiConnection
from .realtime.struct import (
    # session & items
    AssistantMessageItemParam,
    SessionCreated,
    SessionUpdated,
    ItemCreated,
    ItemCreate,
    ItemInputAudioTranscriptionDelta,
    ItemInputAudioTranscriptionCompleted,
    ItemInputAudioTranscriptionFailed,
    # responses
    ResponseCreated,
    ResponseDone,
    ResponseAudioTranscriptDelta,
    ResponseAudioTranscriptDone,
    ResponseTextDelta,
    ResponseTextDone,
    ResponseAudioDelta,
    ResponseAudioDone,
    ResponseOutputItemAdded,
    ResponseOutputItemDone,
    # VAD / speech state
    InputAudioBufferSpeechStarted,
    InputAudioBufferSpeechStopped,
    # tools
    ResponseFunctionCallArgumentsDone,
    FunctionCallOutputItemParam,
    # config/update
    SessionUpdate,
    SessionUpdateParams,
    InputAudioTranscription,
    AzureInputAudioTranscription,
    AzureInputAudioNoiseReduction,
    AzureInputAudioEchoCancellation,
    AzureSemanticVadUpdateParams,
    ServerVADUpdateParams,
    AzureVoice,
    ContentType,
    ResponseCreate,
    ErrorMessage,
    UserMessageItemParam,
)


@dataclass
class AzureRealtimeConfig(BaseModel):
    base_url: str = ""
    api_key: str = ""
    path: str = "/voice-live/realtime"
    model: str = (
        "gpt-4o"  # supports both gpt-4o(-mini)-realtime-* or chat models
    )
    api_version: str = "2025-05-01-preview"
    language: str = "en-US"
    prompt: str = ""
    temperature: float = 0.5
    max_tokens: int = 1024

    # TTS / Voice
    voice_name: str = "en-US-AndrewMultilingualNeural"
    voice_type: str = "azure-standard"
    voice_endpoint: str | None = None
    voice_temperature: float = 0.8

    # Output & VAD
    audio_out: bool = True
    server_vad: bool = (
        True  # for gpt-4o-realtime* models; otherwise Azure semantic VAD is used
    )
    sample_rate: int = 24000

    # Transcription (input ASR)
    input_transcript: bool = True

    # Front-end processing
    input_audio_noise_reduction: bool = True
    input_audio_echo_cancellation: bool = False

    # Misc
    vendor: str = ""  # parity with OpenAI impl (not used by Azure)
    dump: bool = False
    dump_path: str = ""


class AzureRealtime2Extension(AsyncMLLMBaseExtension):
    """
    Azure realtime provider, API-compatible with OpenAIRealtime2Extension.
    - Minimal, predictable loop (no ChatMemory/usage stats)
    - Normalized events → send_server_* helpers
    - Tool calls forwarded via send_server_function_call
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.loop: asyncio.AbstractEventLoop | None = None

        self.config: AzureRealtimeConfig | None = None
        self.conn: RealtimeApiConnection | None = None

        self.connected: bool = False
        self.stopped: bool = False
        self.session_id: str | None = None

        self.available_tools: list[LLMToolMetadata] = []
        self.request_transcript: str = ""
        self.response_transcript: str = ""

    # ---------- lifecycle ----------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env
        self.loop = asyncio.get_event_loop()

        properties, _ = await ten_env.get_property_to_json(None)
        self.config = AzureRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if not self.config.api_key or not self.config.base_url:
            ten_env.log_error("api_key and base_url are required")
            raise ValueError("api_key/base_url required")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        self.stopped = True
        if self.conn:
            await self.conn.close()

    def vendor(self) -> str:
        return "azure"

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    async def start_connection(self) -> None:
        try:
            self.conn = RealtimeApiConnection(
                ten_env=self.ten_env,
                base_url=self.config.base_url,
                path=self.config.path,
                api_key=self.config.api_key,
                api_version=self.config.api_version,
                model=self.config.model,
            )
            await self.conn.connect()

            item_id = ""
            response_id = ""
            flushed: set[str] = set()
            session_start_ms = int(time.time() * 1000)

            self.ten_env.log_info("[Azure] client loop started")
            async for message in self.conn.listen():
                try:
                    match message:
                        # ---- session lifecycle ----
                        case SessionCreated():
                            self.connected = True
                            self.session_id = message.session.id
                            self.ten_env.log_info(
                                f"[Azure] session created: {self.session_id}"
                            )
                            await self._update_session()
                            await self._resume_context(self.message_context)

                        case SessionUpdated():
                            self.ten_env.log_info("[Azure] session updated")
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )

                        # ---- input (user ASR) ----
                        case ItemInputAudioTranscriptionDelta():
                            self.ten_env.log_info(
                                f"[Azure] input transcription delta: {message}"
                            )
                            self.request_transcript += message.delta or ""
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=self.request_transcript,
                                    delta=message.delta or "",
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                        case ItemInputAudioTranscriptionCompleted():
                            self.ten_env.log_info(
                                f"[Azure] input transcription completed: {message}"
                            )
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=message.transcript,
                                    delta="",
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.request_transcript = ""
                        case ItemInputAudioTranscriptionFailed():
                            self.ten_env.log_warn(
                                f"[Azure] input transcription failed: {message.error}"
                            )
                            self.request_transcript = ""

                        case ItemCreated():
                            self.ten_env.log_debug(
                                f"[Azure] item created: {message.item}"
                            )

                        # ---- response lifecycle ----
                        case ResponseCreated():
                            response_id = message.response.id
                            self.ten_env.log_debug(
                                f"[Azure] response created: {response_id}"
                            )

                        case ResponseDone():
                            rid = message.response.id
                            status = message.response.status
                            if rid == response_id:
                                response_id = ""
                            self.ten_env.log_debug(
                                f"[Azure] response done {rid} status={status} usage={message.response.usage}"
                            )

                        # ---- assistant streaming text/ASR ----
                        case ResponseAudioTranscriptDelta():
                            if message.response_id in flushed:
                                continue
                            self.response_transcript += message.delta or ""
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta=message.delta or "",
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                        case ResponseTextDelta():
                            if message.response_id in flushed:
                                continue
                            if item_id != message.item_id:
                                item_id = message.item_id
                            self.response_transcript += message.delta or ""
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript,
                                    delta=message.delta or "",
                                    final=False,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )

                        case ResponseAudioTranscriptDone():
                            if message.response_id in flushed:
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

                        case ResponseTextDone():
                            if message.response_id in flushed:
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

                        # ---- assistant TTS audio ----
                        case ResponseAudioDelta():
                            if message.response_id in flushed:
                                continue
                            if item_id != message.item_id:
                                item_id = message.item_id
                            audio_bytes = base64.b64decode(message.delta)
                            await self.send_server_output_audio_data(
                                audio_bytes
                            )

                        case ResponseAudioDone():
                            # nothing special; text/audio done events above already finalize segments
                            pass

                        case ResponseOutputItemAdded():
                            self.ten_env.log_debug(
                                f"[Azure] output item added idx={message.output_index} item={message.item}"
                            )
                        case ResponseOutputItemDone():
                            self.ten_env.log_debug(
                                f"[Azure] output item done {message.item}"
                            )

                        # ---- VAD notifications from server ----
                        case InputAudioBufferSpeechStarted():
                            self.ten_env.log_info(
                                f"[Azure] server VAD: speech started in response {response_id}, last item {item_id}"
                            )
                            # recompute relative timing for truncation if needed
                            current_ms = int(time.time() * 1000)
                            _ = current_ms - session_start_ms
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
                            # update base time using server-reported end offset for later truncate if needed
                            session_start_ms = (
                                int(time.time() * 1000) - message.audio_end_ms
                            )
                            self.ten_env.log_info(
                                f"[Azure] server VAD: speech stopped, audio_end_ms={message.audio_end_ms}"
                            )

                        # ---- tool call ----
                        case ResponseFunctionCallArgumentsDone():
                            self.ten_env.log_info(
                                f"[Azure] tool call requested: {message.name}"
                            )
                            # forward to host; host will reply via send_client_function_call_output
                            await self.send_server_function_call(
                                MLLMServerFunctionCall(
                                    call_id=message.call_id,
                                    name=message.name,
                                    arguments=message.arguments,
                                )
                            )

                        # ---- errors ----
                        case ErrorMessage():
                            self.ten_env.log_error(
                                f"[Azure] error: {message.error}"
                            )

                        case _:
                            self.ten_env.log_debug(
                                f"[Azure] unhandled message: {message}"
                            )

                except Exception as e:
                    traceback.print_exc()
                    self.ten_env.log_error(
                        f"[Azure] error processing message {message}: {e}"
                    )

            self.ten_env.log_info("[Azure] client loop finished")
        except Exception as e:
            traceback.print_exc()
            self.ten_env.log_error(f"[Azure] start_connection failed: {e}")

        await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.connected = False
        if self.conn:
            await self.conn.close()
        self.stopped = True

    async def _handle_reconnect(self) -> None:
        # follow OpenAI style: close, small backoff, reconnect while not stopped
        await self.stop_connection()
        if not self.stopped:
            await asyncio.sleep(1.0)
            await self.start_connection()

    def is_connected(self) -> bool:
        return self.connected

    # ---------- client → provider ----------

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        self.session_id = session_id
        if not self.conn:
            return False
        await self.conn.send_audio_data(frame.get_buf())
        return True

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        await super().on_data(ten_env, data)

    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        if not self.conn:
            return
        match item.role:
            case "user":
                await self.conn.send_request(
                    ItemCreate(
                        item=UserMessageItemParam(
                            content=[
                                {
                                    "type": ContentType.InputText,
                                    "text": item.content or "",
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
                                {
                                    "type": ContentType.Text,
                                    "text": item.content or "",
                                }
                            ]
                        )
                    )
                )
            case _:
                self.ten_env.log_error(f"[Azure] unknown role: {item.role}")

    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        if not self.conn:
            return
        await self.conn.send_request(ResponseCreate())

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        self.available_tools.append(tool)
        await self._update_session()

    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        # Azure expects tool result as an item with FunctionCallOutputItemParam, then create a response.
        if not self.conn:
            return
        await self.conn.send_request(
            ItemCreate(
                item=FunctionCallOutputItemParam(
                    call_id=function_call_output.call_id,
                    output=function_call_output.output,
                )
            )
        )
        await self.conn.send_request(ResponseCreate())

    async def _resume_context(
        self, messages: list[MLLMClientMessageItem]
    ) -> None:
        for m in messages:
            try:
                await self.send_client_message_item(m)
            except Exception:
                pass

    # ---------- session update ----------

    async def _update_session(self) -> None:
        if not self.connected or not self.conn:
            self.ten_env.log_warn("[Azure] not connected; skip session update")
            return

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

        tools = (
            [tool_dict(t) for t in self.available_tools]
            if self.available_tools
            else []
        )
        prompt = self.config.prompt

        # Default: use Azure semantic VAD for non-realtime chat models,
        # and server VAD for gpt-4o-realtime* models.
        if self.config.model in (
            "gpt-4o-realtime-preview",
            "gpt-4o-mini-realtime-preview",
        ):
            vad_params = (
                ServerVADUpdateParams() if self.config.server_vad else None
            )
        else:
            vad_params = AzureSemanticVadUpdateParams()

        su = SessionUpdate(
            session=SessionUpdateParams(
                instructions=prompt,
                model=self.config.model,
                tool_choice="auto" if self.available_tools else "none",
                tools=tools,
                turn_detection=vad_params,
                input_audio_noise_reduction=(
                    AzureInputAudioNoiseReduction()
                    if self.config.input_audio_noise_reduction
                    else None
                ),
                input_audio_echo_cancellation=(
                    AzureInputAudioEchoCancellation()
                    if self.config.input_audio_echo_cancellation
                    else None
                ),
            )
        )

        # output modality / voice
        if self.config.audio_out:
            su.session.voice = AzureVoice(
                name=self.config.voice_name,
                type=self.config.voice_type,
                temperature=self.config.voice_temperature,
                endpoint_id=self.config.voice_endpoint,
            )
        else:
            su.session.modalities = ["text"]

        # input transcription
        if self.config.input_transcript:
            if self.config.model in (
                "gpt-4o-realtime-preview",
                "gpt-4o-mini-realtime-preview",
            ):
                su.session.input_audio_transcription = InputAudioTranscription()
            else:
                su.session.input_audio_transcription = (
                    AzureInputAudioTranscription()
                )

        await self.conn.send_request(su)
