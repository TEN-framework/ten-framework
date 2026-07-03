#
# Agora Real Time Engagement
# GLM Realtime MLLM — aligned to OpenAIRealtime2Extension pattern
# Created by Wei Hu in 2024-08. Refactor by <you>.
#
import asyncio
import base64
import io
import json
import traceback
from dataclasses import dataclass
from enum import Enum
from typing import Iterable

from pydantic import BaseModel
from pydub import AudioSegment

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
from ten_ai_base.types import LLMToolMetadata, LLMChatCompletionContentPartParam

from .realtime.connection import RealtimeApiConnection
from .realtime.struct import (
    # session & items
    AssistantMessageItemParam,
    SessionCreated,
    SessionUpdated,  # GLM may not emit; kept for parity
    ItemCreated,
    ItemCreate,
    ItemInputAudioTranscriptionCompleted,
    ItemInputAudioTranscriptionFailed,
    # responses
    ResponseCreated,
    ResponseDone,
    ResponseAudioTranscriptDelta,
    ResponseAudioTranscriptDone,  # GLM seldom sends; we still handle
    ResponseAudioDelta,
    ResponseAudioDone,
    ResponseOutputItemAdded,
    ResponseOutputItemDone,
    # vad
    InputAudioBufferSpeechStarted,
    InputAudioBufferSpeechStopped,
    # tools
    ResponseFunctionCallArgumentsDone,
    FunctionCallOutputItemParam,
    # config/update
    SessionUpdate,
    SessionUpdateParams,
    ContentType,
    ResponseCreate,
    AudioFormats,
    ErrorMessage,
    UserMessageItemParam,
)


# ------------------------------
# Config
# ------------------------------
class Role(str, Enum):
    User = "user"
    Assistant = "assistant"


@dataclass
class GLMRealtimeConfig(BaseModel):
    base_url: str = "wss://open.bigmodel.cn"
    api_key: str = ""
    path: str = "/api/paas/v4/realtime"

    prompt: str = ""
    temperature: float = 0.5
    max_tokens: int = 1024

    server_vad: bool = True
    audio_out: bool = True
    input_transcript: bool = True
    sample_rate: int = 24000
    language: str = "en-US"

    # misc
    dump: bool = False
    dump_path: str = ""


# ------------------------------
# Extension
# ------------------------------
class GLMRealtime2Extension(AsyncMLLMBaseExtension):
    """
    Zhipu GLM realtime provider, following OpenAIRealtime2Extension shape.
    - start_connection/stop_connection + reconnect
    - normalized send_server_* bridges
    - GLM requires WAV on input; we buffer PCM and push WAV chunks
    - GLM tool calls do NOT include call_id; we return results without call_id
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.loop: asyncio.AbstractEventLoop | None = None

        self.config: GLMRealtimeConfig | None = None
        self.conn: RealtimeApiConnection | None = None

        self.stopped: bool = False
        self.connected: bool = False
        self.session_id: str | None = None

        self.available_tools: list[LLMToolMetadata] = []

        # streaming state
        self.response_transcript = ""
        self.request_transcript = ""

        self._pcm_buffer = bytearray()
        self._last_send_ts = 0.0
        self._qps_limit = 50  # 最大发送频率

    # ---------- lifecycle ----------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        self.ten_env = ten_env
        self.loop = asyncio.get_event_loop()

        properties, _ = await ten_env.get_property_to_json(None)
        self.config = GLMRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if not self.config.api_key:
            ten_env.log_error("api_key is required")
            raise ValueError("api_key is required")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        self.stopped = True
        if self.conn:
            await self.conn.close()

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def vendor(self) -> str:
        return "glm"

    async def start_connection(self) -> None:
        try:
            self.conn = RealtimeApiConnection(
                ten_env=self.ten_env,
                base_url=self.config.base_url,
                path=self.config.path,
                api_key=self.config.api_key,
            )
            await self.conn.connect()

            response_id = ""
            flushed: set[str] = set()

            self.ten_env.log_info("[GLM] client loop started")
            async for message in self.conn.listen():
                try:
                    match message:
                        # ---- session lifecycle ----
                        case SessionCreated():
                            self.connected = True
                            self.session_id = message.session.id
                            self.ten_env.log_info(
                                f"[GLM] session created: {self.session_id}"
                            )
                            await self._update_session()
                            await self._resume_context(self.message_context)
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )

                        case SessionUpdated():
                            # GLM may not emit; keep for parity
                            self.ten_env.log_debug("[GLM] session updated")
                            await self.send_server_session_ready(
                                MLLMServerSessionReady()
                            )

                        case ItemCreated():
                            self.ten_env.log_debug(
                                f"[GLM] item created {message.item}"
                            )

                        # ---- responses lifecycle ----
                        case ResponseCreated():
                            response_id = message.response.id
                            self.ten_env.log_debug(
                                f"[GLM] response created {response_id}"
                            )

                        case ResponseDone():
                            rid = message.response.id
                            if rid == response_id:
                                response_id = ""
                            # GLM sometimes lacks transcript-done; finalize here.
                            await self._finalize_output_if_needed()
                            self.ten_env.log_debug(f"[GLM] response done {rid}")

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

                        case ResponseAudioTranscriptDone():
                            if message.response_id in flushed:
                                continue
                            await self.send_server_output_text(
                                MLLMServerOutputTranscript(
                                    content=self.response_transcript
                                    or (message.transcript or ""),
                                    delta="",
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.response_transcript = ""

                        # ---- assistant audio ----
                        case ResponseAudioDelta():
                            audio_bytes = base64.b64decode(message.delta)
                            await self.send_server_output_audio_data(
                                audio_bytes
                            )

                        case ResponseAudioDone():
                            # no-op
                            pass

                        case ResponseOutputItemAdded():
                            self.ten_env.log_debug(
                                f"[GLM] output item added {message.output_index} {message.item}"
                            )
                        case ResponseOutputItemDone():
                            self.ten_env.log_debug(
                                f"[GLM] output item done {message.item}"
                            )

                        # ---- input (user ASR) ----
                        case ItemInputAudioTranscriptionCompleted():
                            txt = message.transcript or ""
                            await self.send_server_input_transcript(
                                MLLMServerInputTranscript(
                                    content=txt,
                                    delta=txt,
                                    final=True,
                                    metadata={
                                        "session_id": self.session_id or "-1"
                                    },
                                )
                            )
                            self.request_transcript = ""

                        case ItemInputAudioTranscriptionFailed():
                            self.ten_env.log_warn(
                                f"[GLM] input transcription failed: {message.error}"
                            )
                            self.request_transcript = ""

                        # ---- server VAD ----
                        case InputAudioBufferSpeechStarted():
                            # interrupt current assistant output
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

                        case InputAudioBufferSpeechStopped():
                            # nothing extra; your pipeline can treat this as end-of-user-turn if needed
                            self.ten_env.log_debug("[GLM] server VAD: stopped")

                        # ---- tools ----
                        case ResponseFunctionCallArgumentsDone():
                            # GLM does not provide call_id; forward to host
                            await self.send_server_function_call(
                                MLLMServerFunctionCall(
                                    call_id="",  # no call_id from GLM
                                    name=message.name,
                                    arguments=message.arguments,
                                )
                            )

                        # ---- errors ----
                        case ErrorMessage():
                            self.ten_env.log_error(
                                f"[GLM] error: {message.error}"
                            )

                        case _:
                            self.ten_env.log_debug(
                                f"[GLM] unhandled message: {message}"
                            )

                except Exception as e:
                    traceback.print_exc()
                    self.ten_env.log_error(
                        f"[GLM] error processing message {message}: {e}"
                    )

            self.ten_env.log_info("[GLM] client loop finished")
        except Exception as e:
            traceback.print_exc()
            self.ten_env.log_error(f"[GLM] start_connection failed: {e}")

        await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.connected = False
        if self.conn:
            await self.conn.close()
        self.stopped = True

    async def _handle_reconnect(self) -> None:
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
        """GLM expects WAV; buffer PCM and periodically send small WAV chunks."""
        self.session_id = session_id
        if not self.connected or not self.conn:
            return False

        pcm = frame.get_buf()
        self._pcm_buffer.extend(pcm)

        now = asyncio.get_event_loop().time()
        min_interval = 1.0 / self._qps_limit
        if now - self._last_send_ts >= min_interval:
            await self.conn.send_audio_data(bytes(self._pcm_buffer))
            self._pcm_buffer.clear()
            self._last_send_ts = now
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
                self.ten_env.log_error(f"[GLM] unknown role: {item.role}")

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
        """GLM tool output has no call_id field; return as FunctionCallOutputItemParam(output=...)."""
        if not self.conn:
            return
        await self.conn.send_request(
            ItemCreate(
                item=FunctionCallOutputItemParam(
                    output=(
                        json.dumps(
                            self._convert_to_content_parts(
                                function_call_output.output
                            )
                        )
                        if not isinstance(function_call_output.output, str)
                        else function_call_output.output
                    )
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

    # ---------- helpers ----------

    async def _finalize_output_if_needed(self) -> None:
        """Ensure we emit a final segment if GLM skipped transcript-done."""
        if self.response_transcript:
            await self.send_server_output_text(
                MLLMServerOutputTranscript(
                    content=self.response_transcript,
                    delta="",
                    final=True,
                    metadata={"session_id": self.session_id or "-1"},
                )
            )
            self.response_transcript = ""

    def _pcm_to_wav_bytes(self, pcm: bytes | bytearray, sr: int) -> bytes:
        """Wrap raw PCM int16 mono into WAV (in-memory) using pydub."""
        seg = AudioSegment(pcm, frame_rate=sr, sample_width=2, channels=1)
        bio = io.BytesIO()
        seg.export(bio, format="wav")
        return bio.getvalue()

    def _convert_to_content_parts(
        self, content: Iterable[LLMChatCompletionContentPartParam] | str
    ):
        if isinstance(content, str):
            return [{"type": "text", "text": content}]
        parts = []
        for p in content:
            if isinstance(p, dict) and p.get("type") == "text":
                parts.append(p)
        return parts

    # ---------- session update ----------

    async def _update_session(self) -> None:
        if not self.connected or not self.conn:
            self.ten_env.log_warn("[GLM] not connected; skip session update")
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
        su = SessionUpdate(
            session=SessionUpdateParams(
                instructions=self.config.prompt,
                input_audio_format=AudioFormats.PCM,  # GLM needs WAV input
                output_audio_format=AudioFormats.PCM,
                tools=tools,
            )
        )

        await self.conn.send_request(su)
