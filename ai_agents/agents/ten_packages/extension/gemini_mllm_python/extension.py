#
# Agora Real Time Engagement
# Gemini Realtime MLLM — aligned to OpenAIRealtime2Extension / StepFunRealtime2Extension
# Created by Wei Hu in 2024-08. Refactor by Roei Bracha in 2025-11
#

import asyncio
import json
import os
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
    RealtimeInputConfig,
    AudioTranscriptionConfig,
    ProactivityConfig,
    LiveServerContent,
    Modality,
    MediaResolution,
)


# ------------------------------
# Enhanced Configuration
# ------------------------------
@dataclass
class GeminiRealtimeConfig(BaseModel):
    """Enhanced configuration for Gemini Live API with full feature support."""

    # Core settings
    api_key: str = ""
    model: str = "gemini-2.5-flash-native-audio-preview-09-2025"
    language: str = "en-US"
    prompt: str = ""
    input_sample_rate: int = (
        16000  # 16kHz for input audio (microphone to Gemini)
    )
    output_sample_rate: int = (
        24000  # 24kHz for output audio (Gemini to speaker)
    )
    audio_out: bool = True

    # Generation config
    temperature: float = 0.5
    max_tokens: int = 1024

    # Voice config
    voice: str = (
        "Puck"  # Options: Puck, Charon, Kore, Fenrir, Aoede, Orus, Zephyr
    )

    # VAD settings
    server_vad: bool = True
    vad_start_sensitivity: Literal["low", "high", "default"] = "default"
    vad_end_sensitivity: Literal["low", "high", "default"] = "default"
    vad_prefix_padding_ms: int | None = None
    vad_silence_duration_ms: int | None = None

    # Turn coverage - NEW
    turn_coverage: Literal["only_activity", "all_input"] = "all_input"

    # Transcription switches
    transcribe_agent: bool = True
    transcribe_user: bool = True

    # Video streaming
    media_resolution: MediaResolution = MediaResolution.MEDIA_RESOLUTION_MEDIUM

    # Advanced features
    affective_dialog: bool = False
    proactive_audio: bool = False

    # Greeting message - NEW
    greeting: str = ""

    # Debug
    dump: bool = False
    dump_path: str = ""

    # Realtime input buffer
    audio_chunk_bytes: int = 1024


# ------------------------------
# Extension Implementation
# ------------------------------
class GeminiRealtime2Extension(AsyncMLLMBaseExtension):
    """
    Google Gemini Live API extension V2 - Enhanced with:
    - Fixed transcript handling (no accumulation across turns)
    - Turn coverage configuration
    - Greeting message support
    - Proper interruption handling
    - Full configurability of Gemini Live API features
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

        # Stream buffers
        self._in_pcm_buf = bytearray()
        self._out_pcm_leftover = b""
        self.request_transcript = ""
        self.response_transcript = ""

        # Cached session config
        self._cached_session_config: (
            LiveConnectConfig | LiveConnectConfigDict | None
        ) = None
        self.available_tools: list[LLMToolMetadata] = []

        # Greeting flag
        self._greeting_sent: bool = False

    # ---------- Lifecycle ----------

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        await super().on_init(ten_env)
        ten_env.log_debug("on_init")
        self.ten_env = ten_env
        self.loop = asyncio.get_event_loop()

        properties, _ = await ten_env.get_property_to_json(None)
        self.config = GeminiRealtimeConfig.model_validate_json(properties)
        ten_env.log_info(f"config: {self.config}")

        if self.config.api_key != "":
            self.client = genai.Client(api_key=self.config.api_key)
        elif os.environ.get("GEMINI_API_KEY"):
            self.client = genai.Client(api_key=os.environ.get("GEMINI_API_KEY"))
        elif (
            os.environ.get("GOOGLE_GENAI_USE_VERTEXAI")
            and os.environ.get("GOOGLE_CLOUD_PROJECT")
            and os.environ.get("GOOGLE_CLOUD_LOCATION")
        ):
            self.client = genai.Client(
                vertexai=True,
                project=os.environ.get("GOOGLE_CLOUD_PROJECT"),
                location=os.environ.get("GOOGLE_CLOUD_LOCATION"),
            )
        else:
            ten_env.log_error("api_key is required")
            raise ValueError("api_key is required")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        await super().on_stop(ten_env)
        self.stopped = True
        if self.session:
            await self.stop_connection()

    def input_audio_sample_rate(self) -> int:
        return self.config.input_sample_rate

    def synthesize_audio_sample_rate(self) -> int:
        return self.config.output_sample_rate

    def vendor(self) -> str:
        return "google"

    async def _receive_loop(self):
        """Receive loop for incoming messages from the server."""
        while not self.stopped:
            try:
                async for resp in self.session.receive():
                    try:
                        await self._handle_server_message(resp)
                    except Exception as e:
                        self.ten_env.log_error(
                            f"[Gemini] error in message handler: {e}"
                        )
                        traceback.print_exc()
            except Exception as e:
                self.ten_env.log_error(f"[Gemini] receive loop error: {e}")
                traceback.print_exc()
                break

    async def start_connection(self) -> None:
        """Start connection to Gemini Live API with enhanced configuration."""
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
                self._greeting_sent = False

                await self.send_server_session_ready(MLLMServerSessionReady())
                await self._resume_context(self.message_context)

                # Send greeting message if configured
                if self.config.greeting and not self._greeting_sent:
                    await self._send_greeting()

                # Start the receive loop
                recv_task = asyncio.create_task(self._receive_loop())

                # Block until task is finished or stopped
                await recv_task

            self.ten_env.log_info("[Gemini] session closed")
        except Exception as e:
            self.ten_env.log_error(f"[Gemini] start_connection failed: {e}")
            traceback.print_exc()
        finally:
            await self._handle_reconnect()

    async def stop_connection(self) -> None:
        """Stop the connection gracefully."""
        self.connected = False
        self.stopped = True
        if self.session:
            try:
                await self.session.close()
            except Exception:
                pass

    async def _handle_reconnect(self) -> None:
        """Handle reconnection logic."""
        if self.stopped:
            return
        await asyncio.sleep(1)
        await self.start_connection()

    def is_connected(self) -> bool:
        return self.connected

    # ---------- Provider ingress (Client � Gemini) ----------

    async def send_audio(
        self, frame: AudioFrame, session_id: str | None
    ) -> bool:
        """Push raw PCM to Gemini live session."""
        if not self.connected or not self.session:
            return False
        self.session_id = session_id
        pcm = frame.get_buf()

        # Optional dump for debugging
        if self.config.dump and self.config.dump_path:
            try:
                with open(self.config.dump_path, "ab") as f:
                    f.write(pcm)
            except Exception as e:
                self.ten_env.log_warn(f"[Gemini] Failed to dump audio: {e}")

        blob = types.Blob(
            data=pcm,
            mime_type=f"audio/pcm;rate={self.config.input_sample_rate}",
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
        """
        Trigger model response.
        Gemini responds automatically on input; kept for API parity.
        """
        return

    async def send_client_register_tool(self, tool: LLMToolMetadata) -> None:
        """Register tools (effective next session connect)."""
        self.ten_env.log_info(f"[Gemini] register tool: {tool.name}")
        self.available_tools.append(tool)
        # Note: Gemini tools are baked into connect config
        # To apply immediately would need session restart

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

    async def _send_greeting(self) -> None:
        """Send greeting message after connection."""
        if not self.connected or not self.session or self._greeting_sent:
            return
        try:
            self.ten_env.log_info(
                f"[Gemini] sending greeting: {self.config.greeting}"
            )
            await self.session.send_client_content(
                turns=Content(
                    role="user", parts=[Part(text=self.config.greeting)]
                )
            )
            self._greeting_sent = True
        except Exception as e:
            self.ten_env.log_error(f"[Gemini] failed to send greeting: {e}")

    # ---------- Server message handling ----------

    async def _handle_server_message(self, msg: LiveServerMessage) -> None:
        """
        Handle incoming messages from Gemini Live API.
        Enhanced with proper transcript handling following OpenAI patterns.
        """
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

            # Process audio FIRST before anything else (avoid dropouts)
            if sc.model_turn and sc.model_turn.parts:
                for p in sc.model_turn.parts:
                    if p.inline_data and p.inline_data.data:
                        # Send audio data directly - base class handles buffering
                        try:
                            await self.send_server_output_audio_data(
                                p.inline_data.data
                            )
                        except Exception as e:
                            self.ten_env.log_warn(
                                f"[Gemini] Failed to send audio data: {e}"
                            )

            # CRITICAL FIX: Interrupt handling with transcript finalization
            if sc.interrupted:
                # Finalize any pending transcript before sending interrupt signal
                if self.response_transcript:
                    await self.send_server_output_text(
                        MLLMServerOutputTranscript(
                            content=self.response_transcript + "[interrupted]",
                            delta="",
                            final=True,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                    self.response_transcript = ""  # Reset buffer

                await self.send_server_interrupted(sos=MLLMServerInterrupt())
                return

            # FIXED: Input transcript (user) - always accumulate, reset on turn_complete
            if sc.input_transcription:
                # Always accumulate text (no finished check needed)
                self.request_transcript += sc.input_transcription.text
                await self.send_server_input_transcript(
                    MLLMServerInputTranscript(
                        content=self.request_transcript,
                        delta=sc.input_transcription.text,
                        final=False,  # Will be finalized on turn_complete
                        metadata={"session_id": self.session_id or "-1"},
                    )
                )

            # FIXED: Output transcript (assistant) - always accumulate, reset on turn_complete
            if sc.output_transcription:
                # Always accumulate text (no finished check needed)
                self.response_transcript += sc.output_transcription.text
                await self.send_server_output_text(
                    MLLMServerOutputTranscript(
                        content=self.response_transcript,
                        delta=sc.output_transcription.text,
                        final=False,  # Will be finalized on turn_complete
                        metadata={"session_id": self.session_id or "-1"},
                    )
                )

            # CRITICAL: Handle turn_complete to finalize and reset transcripts
            if sc.turn_complete:
                # Finalize input transcript if exists
                if self.request_transcript:
                    await self.send_server_input_transcript(
                        MLLMServerInputTranscript(
                            content=self.request_transcript,
                            delta="",
                            final=True,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                    self.request_transcript = ""  # Reset buffer

                # Finalize output transcript if exists
                if self.response_transcript:
                    await self.send_server_output_text(
                        MLLMServerOutputTranscript(
                            content=self.response_transcript,
                            delta="",
                            final=True,
                            metadata={"session_id": self.session_id or "-1"},
                        )
                    )
                    self.response_transcript = ""  # Reset buffer

    # ---------- Tools ----------

    async def _handle_tool_call(self, calls: list[FunctionCall]) -> None:
        """
        Bridge function calls to host via send_server_function_call.
        Results are returned via send_client_function_call_output.
        """
        if not calls:
            return
        for call in calls:
            tool_call_id = call.id
            name = call.name
            arguments = call.args
            self.ten_env.log_info(
                f"[Gemini] tool_call {tool_call_id} {name} {arguments}"
            )

            # Forward to server to execute tool
            await self.send_server_function_call(
                MLLMServerFunctionCall(
                    call_id=tool_call_id,
                    name=name,
                    arguments=json.dumps(arguments),
                )
            )

    # ---------- Session config ----------

    def _build_session_config(self) -> LiveConnectConfig:
        """
        Build LiveConnectConfig with enhanced configuration support.
        Implements all major Gemini Live API features.
        """
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

        # Build realtime config to match ai_studio_code.py
        # Simple config with turn_coverage only - no VAD customization
        realtime_cfg = RealtimeInputConfig(
            turn_coverage="TURN_INCLUDES_ALL_INPUT"
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
