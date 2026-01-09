#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""
OpenTelemetry tracing module for TEN Agent.

This module provides tracing functionality similar to LiveKit Agents,
allowing visualization of the complete message flow in a tracing backend.
"""

import os
from contextlib import asynccontextmanager
from typing import Optional

from opentelemetry import trace
from opentelemetry.sdk.trace import TracerProvider
from opentelemetry.sdk.trace.export import BatchSpanProcessor
from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
from opentelemetry.sdk.resources import SERVICE_NAME, Resource
from opentelemetry.trace import Span, SpanKind, StatusCode as OtelStatusCode


# Trace attribute names (following OpenTelemetry semantic conventions)
class TraceAttrs:
    """Trace attribute names for TEN Agent."""

    # Session attributes
    SESSION_ID = "ten.session_id"
    AGENT_NAME = "ten.agent_name"

    # Turn attributes
    TURN_ID = "ten.turn_id"
    USER_INPUT = "ten.user_input"

    # LLM attributes
    LLM_REQUEST_ID = "ten.llm.request_id"
    LLM_MODEL = "gen_ai.request.model"
    LLM_INPUT_TOKENS = "gen_ai.usage.input_tokens"
    LLM_OUTPUT_TOKENS = "gen_ai.usage.output_tokens"
    LLM_RESPONSE_TEXT = "ten.llm.response_text"

    # TTS attributes
    TTS_INPUT_TEXT = "ten.tts.input_text"
    TTS_REQUEST_ID = "ten.tts.request_id"

    # ASR attributes
    ASR_TEXT = "ten.asr.text"
    ASR_FINAL = "ten.asr.is_final"

    # Tool attributes
    TOOL_NAME = "ten.tool.name"
    TOOL_ARGUMENTS = "ten.tool.arguments"
    TOOL_OUTPUT = "ten.tool.output"
    TOOL_IS_ERROR = "ten.tool.is_error"

    # General attributes
    INTERRUPTED = "ten.interrupted"


class TracingConfig:
    """Configuration for tracing."""
    def __init__(
        self,
        enabled: bool = True,
        otlp_endpoint: str = "http://localhost:4317",
        service_name: str = "ten-voice-agent",
    ):
        self.enabled = enabled
        self.otlp_endpoint = otlp_endpoint
        self.service_name = service_name

    @classmethod
    def from_env(cls) -> "TracingConfig":
        """Create config from environment variables."""
        return cls(
            enabled=os.getenv("TEN_TRACING_ENABLED", "true").lower() == "true",
            otlp_endpoint=os.getenv("TEN_OTLP_ENDPOINT", "http://localhost:4317"),
            service_name=os.getenv("TEN_SERVICE_NAME", "ten-voice-agent"),
        )


class AgentTracer:
    """
    Tracer wrapper for TEN Agent, providing easy-to-use tracing APIs.

    Key design: Each span stores its own context to ensure proper parent-child
    relationships, regardless of Python's contextvars behavior in async code.
    """

    _instance: Optional["AgentTracer"] = None

    def __init__(self, config: TracingConfig):
        self._config = config
        self._tracer: Optional[trace.Tracer] = None
        self._provider: Optional[TracerProvider] = None

        # Active spans - we store both span AND its context
        self._session_span: Optional[Span] = None
        self._session_ctx = None

        self._turn_span: Optional[Span] = None
        self._turn_ctx = None

        self._llm_span: Optional[Span] = None
        self._llm_ctx = None

        self._asr_span: Optional[Span] = None

        self._tts_span: Optional[Span] = None
        self._tts_ctx = None

        if config.enabled:
            self._initialize_otel()

    @classmethod
    def initialize(cls, config: Optional[TracingConfig] = None) -> "AgentTracer":
        """Initialize the global tracer instance."""
        if config is None:
            config = TracingConfig.from_env()

        cls._instance = cls(config)
        return cls._instance

    @classmethod
    def get_instance(cls) -> Optional["AgentTracer"]:
        """Get the global tracer instance."""
        return cls._instance

    def _initialize_otel(self):
        """Initialize OpenTelemetry tracer and exporter."""
        resource = Resource.create({
            SERVICE_NAME: self._config.service_name,
        })

        self._provider = TracerProvider(resource=resource)

        # Configure OTLP exporter
        otlp_exporter = OTLPSpanExporter(
            endpoint=self._config.otlp_endpoint,
            insecure=True,
        )

        # Configure BatchSpanProcessor with shorter intervals for faster export
        span_processor = BatchSpanProcessor(
            otlp_exporter,
            max_queue_size=2048,
            schedule_delay_millis=1000,  # Export every 1 second
            max_export_batch_size=512,
        )
        self._provider.add_span_processor(span_processor)
        trace.set_tracer_provider(self._provider)

        self._tracer = trace.get_tracer(self._config.service_name)

    def force_flush(self, timeout_millis: int = 5000) -> bool:
        """Force flush all pending spans to the exporter."""
        if self._provider:
            return self._provider.force_flush(timeout_millis)
        return False

    @property
    def enabled(self) -> bool:
        return self._config.enabled and self._tracer is not None

    @property
    def tracer(self) -> Optional[trace.Tracer]:
        return self._tracer

    # ==================== Session Level ====================

    def start_session(self, session_id: str, **attrs) -> Optional[Span]:
        """Start the root session span."""
        if not self.enabled:
            return None

        # Create session span (root, no parent)
        self._session_span = self._tracer.start_span(
            "agent_session",
            kind=SpanKind.SERVER,
        )
        self._session_span.set_attribute(TraceAttrs.SESSION_ID, session_id)

        for key, value in attrs.items():
            self._session_span.set_attribute(key, value)

        # Store session context for child spans
        self._session_ctx = trace.set_span_in_context(self._session_span)

        return self._session_span

    def end_session(self, status: str = "ok", **attrs):
        """End the session span."""
        # End any child spans first
        self.end_turn()

        if self._session_span:
            for key, value in attrs.items():
                self._session_span.set_attribute(key, value)

            if status == "error":
                self._session_span.set_status(OtelStatusCode.ERROR)

            self._session_span.end()
            self._session_span = None
            self._session_ctx = None

    def add_session_event(self, name: str, **attrs):
        """Add an event to the session span."""
        if self._session_span:
            self._session_span.add_event(name, attrs)

    # ==================== Turn Level ====================

    def has_active_turn(self) -> bool:
        """Check if there is an active turn span."""
        return self._turn_span is not None

    def start_turn(self, turn_id: int, user_input: str = "", **attrs) -> Optional[Span]:
        """Start a user turn span."""
        if not self.enabled:
            return None

        # End previous turn if exists
        self.end_turn()

        # Create turn span with session as parent
        self._turn_span = self._tracer.start_span(
            "user_turn",
            context=self._session_ctx,  # Explicit parent context
            kind=SpanKind.INTERNAL,
        )
        self._turn_span.set_attribute(TraceAttrs.TURN_ID, turn_id)
        if user_input:
            self._turn_span.set_attribute(TraceAttrs.USER_INPUT, user_input)

        for key, value in attrs.items():
            self._turn_span.set_attribute(key, value)

        # Store turn context for child spans (llm, tts)
        self._turn_ctx = trace.set_span_in_context(self._turn_span)

        return self._turn_span

    def end_turn(self, status: str = "ok", **attrs):
        """End the current turn span."""
        # End any child spans first
        self.end_llm()
        self.end_tts()

        if self._turn_span:
            for key, value in attrs.items():
                self._turn_span.set_attribute(key, value)

            if status == "error":
                self._turn_span.set_status(OtelStatusCode.ERROR)

            self._turn_span.end()
            self._turn_span = None
            self._turn_ctx = None

    def mark_interrupted(self):
        """Mark the current turn as interrupted."""
        if self._turn_span:
            self._turn_span.set_attribute(TraceAttrs.INTERRUPTED, True)
            self._turn_span.add_event("interrupted")

    # ==================== LLM Level ====================

    def start_llm(self, request_id: str = "", **attrs) -> Optional[Span]:
        """Start an LLM inference span."""
        if not self.enabled:
            return None

        # Create LLM span with turn as parent
        self._llm_span = self._tracer.start_span(
            "llm_node",
            context=self._turn_ctx,  # Explicit parent context
            kind=SpanKind.CLIENT,
        )
        if request_id:
            self._llm_span.set_attribute(TraceAttrs.LLM_REQUEST_ID, request_id)

        for key, value in attrs.items():
            self._llm_span.set_attribute(key, value)

        # Store LLM context for child spans (tool calls)
        self._llm_ctx = trace.set_span_in_context(self._llm_span)

        return self._llm_span

    def end_llm(self, status: str = "ok", response_text: str = "", **attrs):
        """End the LLM span."""
        if self._llm_span:
            if response_text:
                self._llm_span.set_attribute(TraceAttrs.LLM_RESPONSE_TEXT, response_text[:1000])

            for key, value in attrs.items():
                self._llm_span.set_attribute(key, value)

            if status == "error":
                self._llm_span.set_status(OtelStatusCode.ERROR)

            self._llm_span.end()
            self._llm_span = None
            self._llm_ctx = None

    def add_llm_event(self, name: str, **attrs):
        """Add an event to the LLM span."""
        if self._llm_span:
            self._llm_span.add_event(name, attrs)

    # ==================== ASR Level ====================

    def start_asr(self, request_id: str = "", **attrs) -> Optional[Span]:
        """Start an ASR recognition span."""
        if not self.enabled:
            return None

        # Create ASR span with turn as parent (or session if no turn yet)
        parent_ctx = self._turn_ctx if self._turn_ctx else self._session_ctx

        self._asr_span = self._tracer.start_span(
            "asr_node",
            context=parent_ctx,
            kind=SpanKind.CLIENT,
        )
        if request_id:
            self._asr_span.set_attribute("ten.asr.request_id", request_id)

        for key, value in attrs.items():
            self._asr_span.set_attribute(key, value)

        return self._asr_span

    def end_asr(self, status: str = "ok", text: str = "", is_final: bool = False, **attrs):
        """End the ASR span."""
        if self._asr_span:
            if text:
                self._asr_span.set_attribute(TraceAttrs.ASR_TEXT, text[:500])
            self._asr_span.set_attribute(TraceAttrs.ASR_FINAL, is_final)

            for key, value in attrs.items():
                self._asr_span.set_attribute(key, value)

            if status == "error":
                self._asr_span.set_status(OtelStatusCode.ERROR)

            self._asr_span.end()
            self._asr_span = None

    def add_asr_event(self, name: str, **attrs):
        """Add an event to the ASR span."""
        if self._asr_span:
            self._asr_span.add_event(name, attrs)

    # ==================== TTS Level ====================

    def start_tts(self, text: str = "", request_id: str = "", **attrs) -> Optional[Span]:
        """Start a TTS synthesis span."""
        if not self.enabled:
            return None

        # Create TTS span with turn as parent
        self._tts_span = self._tracer.start_span(
            "tts_node",
            context=self._turn_ctx,  # Explicit parent context
            kind=SpanKind.CLIENT,
        )
        if text:
            self._tts_span.set_attribute(TraceAttrs.TTS_INPUT_TEXT, text[:500])
        if request_id:
            self._tts_span.set_attribute(TraceAttrs.TTS_REQUEST_ID, request_id)

        for key, value in attrs.items():
            self._tts_span.set_attribute(key, value)

        # Store TTS context
        self._tts_ctx = trace.set_span_in_context(self._tts_span)

        return self._tts_span

    def end_tts(self, status: str = "ok", **attrs):
        """End the TTS span."""
        if self._tts_span:
            for key, value in attrs.items():
                self._tts_span.set_attribute(key, value)

            if status == "error":
                self._tts_span.set_status(OtelStatusCode.ERROR)

            self._tts_span.end()
            self._tts_span = None
            self._tts_ctx = None

    # ==================== Tool Calls ====================

    @asynccontextmanager
    async def trace_tool_call(self, name: str, arguments: str = "", **attrs):
        """Context manager for tracing tool calls."""
        if not self.enabled:
            yield None
            return

        # Create tool span with LLM as parent
        span = self._tracer.start_span(
            "tool_call",
            context=self._llm_ctx,  # Explicit parent context
            kind=SpanKind.CLIENT,
        )
        span.set_attribute(TraceAttrs.TOOL_NAME, name)
        if arguments:
            span.set_attribute(TraceAttrs.TOOL_ARGUMENTS, arguments[:500])

        for key, value in attrs.items():
            span.set_attribute(key, value)

        try:
            yield span
        except Exception as e:
            span.set_status(OtelStatusCode.ERROR, str(e))
            span.set_attribute(TraceAttrs.TOOL_IS_ERROR, True)
            raise
        finally:
            span.end()

    # ==================== Context Managers ====================

    @asynccontextmanager
    async def trace_turn(self, turn_id: int, user_input: str = "", **attrs):
        """Context manager for tracing a complete turn."""
        self.start_turn(turn_id, user_input, **attrs)
        try:
            yield self._turn_span
        except Exception:
            self.end_turn(status="error")
            raise
        else:
            self.end_turn()

    @asynccontextmanager
    async def trace_llm(self, request_id: str = "", **attrs):
        """Context manager for tracing LLM inference."""
        self.start_llm(request_id, **attrs)
        try:
            yield self._llm_span
        except Exception:
            self.end_llm(status="error")
            raise
        else:
            self.end_llm()

    @asynccontextmanager
    async def trace_tts(self, text: str = "", request_id: str = "", **attrs):
        """Context manager for tracing TTS synthesis."""
        self.start_tts(text, request_id, **attrs)
        try:
            yield self._tts_span
        except Exception:
            self.end_tts(status="error")
            raise
        else:
            self.end_tts()

    # ==================== Shutdown ====================

    def shutdown(self):
        """Shutdown the tracer and flush pending spans."""
        self.end_session()

        if self._provider:
            self._provider.force_flush()
            self._provider.shutdown()


# Global tracer instance (lazy initialization)
_tracer: Optional[AgentTracer] = None


def get_tracer() -> Optional[AgentTracer]:
    """Get the global tracer instance."""
    return AgentTracer.get_instance()


def initialize_tracing(config: Optional[TracingConfig] = None) -> AgentTracer:
    """Initialize global tracing."""
    global _tracer
    _tracer = AgentTracer.initialize(config)
    return _tracer
