#
# Agora Real Time Engagement
# Gemini Realtime MLLM — aligned to OpenAIRealtime2Extension / StepFunRealtime2Extension
# Created by Wei Hu in 2024-08. Refactor by <you>.
#
import asyncio
import json
import traceback
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

from google import genai
from google.genai.live import AsyncSession
from google.genai import types
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


# ------------------------------
# Config
# ------------------------------
@dataclass
class GeminiRealtimeConfig(BaseModel):
    api_key: str = ""
    model: str = "gemini-2.0-flash-live-001"
    language: str = "en-US"
    prompt: str = ""
    temperature: float = 0.5
    max_tokens: int = 1024
    voice: str = "Puck"
    server_vad: bool = True
    audio_out: bool = True
    sample_rate: int = 24000
    # realtime input buffer
    audio_chunk_bytes: int = 4096

    # Optional VAD tuning (maps to AutomaticActivityDetection)
    vad_start_sensitivity: Literal["low", "high", "default"] = "default"
    vad_end_sensitivity: Literal["low", "high", "default"] = "default"
    vad_prefix_padding_ms: int | None = None
    vad_silence_duration_ms: int | None = None

    # Transcription switches
    transcribe_agent: bool = True
    transcribe_user: bool = True

    # Video streaming
    media_resolution: MediaResolution = MediaResolution.MEDIA_RESOLUTION_MEDIUM

    # Proactivity / affective dialog flags
    affective_dialog: bool = False
    proactive_audio: bool = False

    # Dump raw audio for debug
    dump: bool = False
    dump_path: str = ""


# ------------------------------
# Extension
# ------------------------------
class GeminiRealtime2Extension(AsyncMLLMBaseExtension):
    """
    Google Gemini realtime provider, API-compatible with OpenAIRealtime2Extension / StepFunRealtime2Extension.
    - Lifecycle: on_init -> start_connection -> listen loop
    - Event mapping -> send_server_* (input/output transcripts, audio data, SOS interrupts, session ready)
    - Tool calls forwarded via send_server_function_call
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv | None = None
        self.loop: asyncio.AbstractEventLoop | None = None

        self.config: GeminiRealtimeConfig | None = None
        self.client: genai.Client | None = None
        self.session: AsyncSession | None = None

        self.stopped: bool = False
        self.connected: bool = False
        self.session_id: str | None = None

        # stream buffers
        self._in_pcm_buf = bytearray()
        self._out_pcm_leftover = b""
        self.request_transcript = ""
        self.response_transcript = ""

        # cached session config
        self._cached_session_config: (
            LiveConnectConfig | LiveConnectConfigDict | None
        ) = None
        self.available_tools: list[LLMToolMetadata] = []

    # ---------- Lifecycle ----------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")
        self.ten_env = ten_env
        self.loop = asyncio.get_event_loop()

        properties, _ = await ten_env.get_property_to_json(None)
        self.config = GeminiRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if not self.config.api_key:
            ten_env.log_error("api_key is required")
            raise ValueError("api_key is required")

        self.client = genai.Client(api_key=self.config.api_key)

    def input_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.sample_rate

    def vendor(self) -> str:
        return "google"

    async def _receive_loop(self):
        """receive loop for incoming messages from the server."""
        while not self.stopped:
            try:
                async for resp in self.session.receive():
                    try:
                        await self._handle_server_message(resp)
                    except Exception as e:
                        self.ten_env.log_error(
                            f"[Gemini] error in message handler: {e}"
                        )
            except Exception as e:
                self.ten_env.log_error(f"[Gemini] receive loop error: {e}")
                break

    async def start_connection(self) -> None:
        await asyncio.sleep(1)
        try:
            cfg = self._build_session_config()
            self.ten_env.log_info(
                f"[Gemini] connecting model={self.config.model}"
            )
            async with self.client.aio.live.connect(
                model=self.config.model, config=cfg
            ) as sess:
                self.session = sess
                self.connected = True
                self.session_id = getattr(self.session, "id", None)

                await self.send_server_session_ready(MLLMServerSessionReady())
                await self._resume_context(self.message_context)

                # start the receive loop
                recv_task = asyncio.create_task(self._receive_loop())

                # block until task is finished or stopped
                await recv_task

            self.ten_env.log_info("[Gemini] session closed")
        except Exception as e:
            self.ten_env.log_error(f"[Gemini] start_connection failed: {e}")
            traceback.print_exc()
        finally:
            await self._handle_reconnect()

    async def stop_connection(self) -> None:
        self.stopped = True
        if self.session:
            try:
                await self.session.close()
            except Exception:
                pass

    async def _handle_reconnect(self) -> None:
        if self.stopped:
            return
        # await asyncio.sleep(1)
        await self.start_connection()

    def is_connected(self) -> bool:
        return self.connected

    # ---------- Provider ingress (Client → Gemini) ----------

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """Push raw PCM to Gemini live session."""
        if not self.connected or not self.session:
            return False
        self.session_id = session_id
        pcm = frame.get_buf()
        # optional dump
        # if self.config.dump: ...
        blob = types.Blob(
            data=pcm,
            # Gemini expects mime type with sample rate. Use config.sample_rate.
            mime_type=f"audio/pcm;rate={self.config.sample_rate}",
        )
        await self.session.send_realtime_input(audio=blob)
        return True

    async def send_client_message_item(
        self, item: MLLMClientMessageItem, session_id: str | None = None
    ) -> None:
        """Send text message as a content turn."""
        if not self.connected or not self.session:
            return
        role = item.role
        text = item.content or ""
        try:
            await self.session.send_client_content(
                turns=Content(role=role, parts=[Part(text=text)])
            )
        except Exception as e:
            self.ten_env.log_error(
                f"[Gemini] send_client_message_item failed: {e}"
            )

    async def send_client_create_response(
        self, session_id: str | None = None
    ) -> None:
        """Trigger model response. Gemini responds automatically on input; keep for API parity."""
        # No explicit trigger needed; send a small control ping to nudge if desired.

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        """Register tools (effective next session connect)."""
        self.ten_env.log_info(f"[Gemini] register tool: {tool.name}")
        self.available_tools.append(tool)
        # Gemini tools are baked in connect config; to apply immediately we'd need a session restart.

    async def send_client_function_call_output(
        self, function_call_output: MLLMClientFunctionCallOutput
    ) -> None:
        """Return tool result back to model (via LiveClientToolResponse)."""
        if not self.connected or not self.session:
            return
        try:
            func_resp = FunctionResponse(
                id=function_call_output.call_id,
                response={"output": function_call_output.output},
            )
            await self.session.send(
                input=LiveClientToolResponse(function_responses=[func_resp])
            )
        except Exception as e:
            self.ten_env.log_error(
                f"[Gemini] send_client_function_call_output failed: {e}"
            )

    async def _resume_context(
        self, messages: list[MLLMClientMessageItem]
    ) -> None:
        """Replay preserved messages into current session."""
        if not self.connected or not self.session:
            return
        for m in messages:
            try:
                await self.send_client_message_item(m)
            except Exception:
                pass

    # ---------- Server message handling ----------

    async def _handle_server_message(self, msg: LiveServerMessage) -> None:
        # Setup done notice
        if msg.setup_complete:
            self.ten_env.log_info("[Gemini] setup complete")
            return

        # Tool calls
        if msg.tool_call and msg.tool_call.function_calls:
            await self._handle_tool_call(msg.tool_call.function_calls)
            return

        # Content stream (audio + transcripts + turn boundaries)
        if msg.server_content:
            sc: LiveServerContent = msg.server_content

            # Interrupt -> send SOS to server side pipeline
            if sc.interrupted:
                await self.send_server_interrupted(sos=MLLMServerInterrupt())
                return

            # Model audio (inline PCM chunks)
            if sc.model_turn and sc.model_turn.parts:
                for p in sc.model_turn.parts:
                    if p.inline_data and p.inline_data.data:
                        await self.send_server_output_audio_data(
                            p.inline_data.data
                        )

            # Input transcript (user)
            if sc.input_transcription:
                if not sc.input_transcription.finished:
                    self.request_transcript += sc.input_transcription.text
                    await self.send_server_input_transcript(
                        MLLMServerInputTranscript(
                            content=self.request_transcript,
                            delta=sc.input_transcription.text,
                            final=False,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                else:
                    # Final input transcript
                    await self.send_server_input_transcript(
                        MLLMServerInputTranscript(
                            content=self.request_transcript,
                            delta="",
                            final=True,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                    self.request_transcript = ""

            # Output transcript (assistant)
            if sc.output_transcription:
                if not sc.output_transcription.finished:
                    self.response_transcript += sc.output_transcription.text
                    await self.send_server_output_text(
                        MLLMServerOutputTranscript(
                            content=self.response_transcript,
                            delta=(
                                sc.output_transcription.text
                                if not sc.turn_complete
                                else ""
                            ),
                            final=bool(sc.output_transcription.finished),
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                else:
                    # Final output transcript
                    await self.send_server_output_text(
                        MLLMServerOutputTranscript(
                            content=self.response_transcript,
                            delta="",
                            final=True,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )

    # ---------- Tools ----------

    async def _handle_tool_call(self, calls: list[FunctionCall]) -> None:
        """Bridge function calls to host via CMD_TOOL_CALL and return results via LiveClientToolResponse."""
        if not calls:
            return
        for call in calls:
            tool_call_id = call.id
            name = call.name
            arguments = call.args
            self.ten_env.log_info(
                f"[Gemini] tool_call {tool_call_id} {name} {arguments}"
            )

            # Forward to server to actually execute user tool
            await self.send_server_function_call(
                MLLMServerFunctionCall(
                    call_id=tool_call_id,
                    name=name,
                    arguments=json.dumps(arguments),
                )
            )

    # ---------- Session config ----------

    def _build_session_config(self) -> LiveConnectConfig:
        if self._cached_session_config is not None:
            return self._cached_session_config  # type: ignore[return-value]

        # Tools from LLMToolMetadata -> Gemini Tool(FunctionDeclaration)
        def tool_decl(t: LLMToolMetadata) -> Tool:
            required: list[str] = []
            props: dict[str, Schema] = {}
            for p in t.parameters:
                props[p.name] = Schema(
                    type=p.type.upper(), description=p.description
                )
                if p.required:
                    required.append(p.name)
            return Tool(
                function_declarations=[
                    FunctionDeclaration(
                        name=t.name,
                        description=t.description,
                        parameters=Schema(
                            type="OBJECT", properties=props, required=required
                        ),
                    )
                ]
            )

        tools = (
            [tool_decl(t) for t in self.available_tools]
            if self.available_tools
            else []
        )

        # VAD mapping
        start_sens = {
            "low": StartSensitivity.START_SENSITIVITY_LOW,
            "high": StartSensitivity.START_SENSITIVITY_HIGH,
            "default": StartSensitivity.START_SENSITIVITY_UNSPECIFIED,
        }[self.config.vad_start_sensitivity]
        end_sens = {
            "low": EndSensitivity.END_SENSITIVITY_LOW,
            "high": EndSensitivity.END_SENSITIVITY_HIGH,
            "default": EndSensitivity.END_SENSITIVITY_UNSPECIFIED,
        }[self.config.vad_end_sensitivity]

        realtime_cfg = RealtimeInputConfig(
            automatic_activity_detection=AutomaticActivityDetection(
                disabled=not self.config.server_vad,
                start_of_speech_sensitivity=start_sens,
                end_of_speech_sensitivity=end_sens,
                prefix_padding_ms=self.config.vad_prefix_padding_ms,
                silence_duration_ms=self.config.vad_silence_duration_ms,
            )
        )

        cfg = LiveConnectConfig(
            response_modalities=(
                [Modality.AUDIO] if self.config.audio_out else [Modality.TEXT]
            ),
            media_resolution=self.config.media_resolution,
            system_instruction=Content(
                parts=[Part(text=self.config.prompt or "")]
            ),
            tools=tools,
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
            realtime_input_config=realtime_cfg,
            output_audio_transcription=(
                AudioTranscriptionConfig()
                if self.config.transcribe_agent
                else None
            ),
            input_audio_transcription=(
                AudioTranscriptionConfig()
                if self.config.transcribe_user
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

        self._cached_session_config = cfg
        return cfg
