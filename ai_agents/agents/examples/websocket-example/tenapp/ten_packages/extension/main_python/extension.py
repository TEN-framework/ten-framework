import asyncio
import json
import time
from typing import Literal

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
)

from .agent.agent import Agent
from .agent.events import (
    ASRResultEvent,
    LLMResponseEvent,
    ToolRegisterEvent,
    UserJoinedEvent,
    UserLeftEvent,
)
from .helper import _send_cmd, _send_data, parse_sentences
from .config import MainControlConfig
from .tracing import AgentTracer, TracingConfig, TraceAttrs, initialize_tracing

import uuid


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module.
    Consumes semantic AgentEvents from the Agent class and drives the runtime behavior.
    Includes OpenTelemetry tracing for observability.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None
        self.agent: Agent = None
        self.config: MainControlConfig = None

        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"

        # Tracing
        self._tracer: AgentTracer = None
        self._llm_response_text: str = ""  # Accumulate LLM response for tracing
        self._tts_started: bool = False

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)

        # Initialize tracing
        # Configure OTLP endpoint to send traces to OpenTelemetry Collector
        # The collector forwards traces to Tempo for visualization in Grafana
        # Default: localhost:4317 (for local grafana-monitoring stack)
        # Remote: Set TEN_OTLP_ENDPOINT environment variable
        import os
        otlp_endpoint = os.getenv("TEN_OTLP_ENDPOINT", "http://10.100.1.211:4317")
        tracing_config = TracingConfig(
            enabled=os.getenv("TEN_TRACING_ENABLED", "true").lower() == "true",
            otlp_endpoint=otlp_endpoint,
            service_name=os.getenv("TEN_SERVICE_NAME", "ten-voice-agent"),
        )
        self._tracer = initialize_tracing(tracing_config)
        ten_env.log_info(f"[MainControlExtension] Tracing initialized, enabled={self._tracer.enabled}")

        self.agent = Agent(ten_env, tracer=self._tracer)

        # Now auto-register decorated methods
        for attr_name in dir(self):
            fn = getattr(self, attr_name)
            event_type = getattr(fn, "_agent_event_type", None)
            if event_type:
                self.agent.on(event_type, fn)

    # === Register handlers with decorators ===
    @agent_event_handler(UserJoinedEvent)
    async def _on_user_joined(self, event: UserJoinedEvent):
        self._rtc_user_count += 1

        # Add user joined event to session span
        self._tracer.add_session_event("user_joined")

        if self._rtc_user_count == 1:
            if self.config and self.config.greeting:
                await self._send_to_tts(self.config.greeting, True)
                await self._send_transcript(
                    "assistant", self.config.greeting, True, 100
                )

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, event: UserLeftEvent):
        self._rtc_user_count -= 1
        self._tracer.add_session_event("user_left")

        # End session when all users leave
        if self._rtc_user_count == 0:
            self._tracer.end_session()

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
        self.session_id = event.metadata.get("session_id", "100")
        stream_id = int(self.session_id)

        self.ten_env.log_info(f"[MainControlExtension] ASR result: text='{event.text}', final={event.final}")

        if not event.text:
            return

        # Interrupt on speech detection
        if event.final or len(event.text) > 2:
            await self._interrupt()

        if event.final:
            # Create ASR span only for final results with text
            asr_id = event.metadata.get("id", f"asr-{self.turn_id}")
            self._tracer.start_asr(
                request_id=asr_id,
                **{"ten.asr.language": event.metadata.get("language", "unknown")}
            )
            self._tracer.end_asr(text=event.text, is_final=True)

            # End previous turn if exists
            self._tracer.end_turn()

            # Start new turn
            self.turn_id += 1
            self.ten_env.log_info(f"[MainControlExtension] Starting turn {self.turn_id} with input: {event.text}")
            self._tracer.start_turn(
                turn_id=self.turn_id,
                user_input=event.text,
            )

            # Reset accumulators
            self._llm_response_text = ""
            self._tts_started = False

            # Start LLM span
            self._tracer.start_llm(request_id=f"llm-{self.turn_id}")

            await self.agent.queue_llm_input(event.text)

        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        # Lazily create turn and LLM spans if not yet created
        # This handles the greeting flow which bypasses ASR
        if not self._tracer.has_active_turn() and event.type == "message":
            self.turn_id += 1
            self.ten_env.log_info(f"[MainControlExtension] Starting turn {self.turn_id} (from LLM greeting)")
            self._tracer.start_turn(
                turn_id=self.turn_id,
                user_input="(greeting)",
            )
            self._llm_response_text = ""
            self._tts_started = False
            self._tracer.start_llm(request_id=f"llm-greeting-{self.turn_id}")

        if not event.is_final and event.type == "message":
            sentences, self.sentence_fragment = parse_sentences(
                self.sentence_fragment, event.delta
            )
            for s in sentences:
                # Start TTS span on first sentence (if not started)
                if not self._tts_started:
                    self._tts_started = True
                    self._tracer.start_tts(
                        text=s,
                        request_id=f"tts-{self.turn_id}"
                    )
                await self._send_to_tts(s, False)

            # Accumulate response text for tracing
            self._llm_response_text += event.delta

        if event.is_final and event.type == "message":
            remaining_text = self.sentence_fragment or ""
            self.sentence_fragment = ""

            # End LLM span with accumulated response
            self._tracer.end_llm(response_text=self._llm_response_text)

            # Add LLM response event
            self._tracer.add_session_event(
                "llm_response_complete",
                response_length=len(self._llm_response_text)
            )

            await self._send_to_tts(remaining_text, True)

            # End TTS span
            self._tracer.end_tts()

            # End turn span and flush to ensure traces are sent
            self._tracer.end_turn()
            self._tracer.force_flush(2000)  # Flush with 2 second timeout
            self.ten_env.log_info(f"[MainControlExtension] Turn {self.turn_id} completed, traces flushed")

        await self._send_transcript(
            "assistant",
            event.text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

        # Start session span immediately when extension starts
        self._tracer.start_session(
            session_id=self.session_id,
            **{TraceAttrs.AGENT_NAME: self.name}
        )
        ten_env.log_info(f"[MainControlExtension] Session span started, session_id={self.session_id}")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

        # Shutdown tracing
        if self._tracer:
            self._tracer.shutdown()
            ten_env.log_info("[MainControlExtension] Tracing shutdown complete")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        await self.agent.on_data(data)

    # === helpers ===
    async def _send_transcript(
        self,
        role: str,
        text: str,
        final: bool,
        stream_id: int,
        data_type: Literal["text", "reasoning"] = "text",
    ):
        """
        Sends the transcript (ASR or LLM output) to the message collector.
        """
        if data_type == "text":
            await _send_data(
                self.ten_env,
                "text_data",
                "websocket_server",
                {
                    "data_type": "transcribe",
                    "role": role,
                    "text": text,
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        elif data_type == "reasoning":
            await _send_data(
                self.ten_env,
                "text_data",
                "websocket_server",
                {
                    "data_type": "raw",
                    "role": role,
                    "text": json.dumps(
                        {
                            "type": "reasoning",
                            "data": {
                                "text": text,
                            },
                        }
                    ),
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
        )

    async def _send_to_tts(self, text: str, is_final: bool):
        """
        Sends a sentence to the TTS system.
        """
        request_id = f"tts-request-{self.turn_id}"
        await _send_data(
            self.ten_env,
            "tts_text_input",
            "tts",
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": is_final,
                "metadata": self._current_metadata(),
            },
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}"
        )

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        # Mark current turn as interrupted in tracing
        self._tracer.mark_interrupted()

        self.sentence_fragment = ""

        # End any active spans due to interruption
        self._tracer.end_llm(status="ok")  # Not an error, just interrupted
        self._tracer.end_tts(status="ok")

        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")
