#
# Agora Real Time Engagement
# Thymia Sentinel WebSocket Integration
# Copyright (c) 2024 Agora IO. All rights reserved.
#
"""
Thymia Analyzer Extension - Sentinel Mode Only

A clean, minimal implementation that streams audio to Thymia's Sentinel
WebSocket API for real-time voice biomarker analysis.

Key features:
- Real-time audio streaming (no local buffering)
- Helios biomarkers: distress, stress, burnout, fatigue, low_self_esteem
- Apollo biomarkers: depression, anxiety (probability + severity)
- Safety classification with alerts
"""
import asyncio
import json
import time
from dataclasses import dataclass
from typing import Optional

from ten_runtime.async_ten_env import AsyncTenEnv
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolResult,
    LLMToolResultLLMResult,
)
from ten_runtime.audio_frame import AudioFrame
from ten_runtime.data import Data

# Sentinel WebSocket API
from .sentinel_client import SentinelClient
from .sentinel_protocol import (
    SentinelConfig,
    PolicyResult,
    StatusMessage,
    ErrorMessage,
)
from .result_mapper import (
    ResultMapper,
    WellnessMetricsCompat,
    SafetyClassification,
)


@dataclass
class WellnessMetrics:
    """Thymia API wellness analysis results"""
    distress: float
    stress: float
    burnout: float
    fatigue: float
    low_self_esteem: float
    timestamp: float
    session_id: str


class ThymiaAnalyzerExtension(AsyncLLMToolBaseExtension):
    """
    Thymia Sentinel extension for real-time voice biomarker analysis.

    Streams audio to Thymia's Sentinel WebSocket API and receives
    real-time biomarker results via callbacks.
    """

    def __init__(self, name: str):
        super().__init__(name)

        # Configuration
        self.api_key: str = ""
        self.ws_url: str = "wss://ws.thymia.ai"
        self.biomarkers: list[str] = ["helios", "apollo"]
        self.policies: list[str] = ["passthrough", "safety_analysis"]
        self.forward_transcripts: bool = True
        self.stream_agent_audio: bool = True
        self.auto_reconnect: bool = True

        # Sentinel client and state
        self.sentinel_client: Optional[SentinelClient] = None
        self.sentinel_latest_result: Optional[PolicyResult] = None
        self.sentinel_wellness: Optional[WellnessMetricsCompat] = None
        self.sentinel_apollo = None  # ApolloResult from result_mapper
        self.sentinel_safety: Optional[SafetyClassification] = None
        self.sentinel_results_count: int = 0

        # Legacy compatibility
        self.latest_results: Optional[WellnessMetrics] = None
        self.apollo_results = None

        # User information for Thymia API
        self.user_name: Optional[str] = None
        self.user_dob: Optional[str] = None
        self.user_sex: Optional[str] = None
        self.user_locale: str = "en-GB"

        # Tracking
        self._audio_frame_count: int = 0
        self.ten_env: Optional[AsyncTenEnv] = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        """Initialize extension and create Sentinel client"""
        ten_env.log_info("[THYMIA] Starting Thymia Analyzer (Sentinel mode)...")
        self.ten_env = ten_env

        # Load configuration
        try:
            api_key_result = await ten_env.get_property_string("api_key")
            self.api_key = (
                api_key_result[0]
                if isinstance(api_key_result, tuple)
                else api_key_result
            )

            try:
                ws_url_result = await ten_env.get_property_string("ws_url")
                self.ws_url = (
                    ws_url_result[0]
                    if isinstance(ws_url_result, tuple)
                    else ws_url_result
                ) or "wss://ws.thymia.ai"
            except Exception:
                self.ws_url = "wss://ws.thymia.ai"

            try:
                biomarkers_result = await ten_env.get_property_string("biomarkers")
                biomarkers_str = (
                    biomarkers_result[0]
                    if isinstance(biomarkers_result, tuple)
                    else biomarkers_result
                )
                self.biomarkers = (
                    [b.strip() for b in biomarkers_str.split(",")]
                    if biomarkers_str
                    else ["helios", "apollo"]
                )
            except Exception:
                self.biomarkers = ["helios", "apollo"]

            try:
                policies_result = await ten_env.get_property_string("policies")
                policies_str = (
                    policies_result[0]
                    if isinstance(policies_result, tuple)
                    else policies_result
                )
                self.policies = (
                    [p.strip() for p in policies_str.split(",")]
                    if policies_str
                    else ["passthrough", "safety_analysis"]
                )
            except Exception:
                self.policies = ["passthrough", "safety_analysis"]

            try:
                self.forward_transcripts = await ten_env.get_property_bool(
                    "forward_transcripts"
                )
            except Exception:
                self.forward_transcripts = True

            try:
                self.stream_agent_audio = await ten_env.get_property_bool(
                    "stream_agent_audio"
                )
            except Exception:
                self.stream_agent_audio = True

            try:
                self.auto_reconnect = await ten_env.get_property_bool(
                    "auto_reconnect"
                )
            except Exception:
                self.auto_reconnect = True

            ten_env.log_info(
                f"[THYMIA] Config: ws_url={self.ws_url}, "
                f"biomarkers={self.biomarkers}, policies={self.policies}"
            )

        except Exception as e:
            ten_env.log_error(f"[THYMIA] Failed to load config: {e}")
            return

        # Create Sentinel client
        self.sentinel_client = SentinelClient(
            ws_url=self.ws_url,
            on_policy_result=lambda r: asyncio.create_task(
                self._on_policy_result(ten_env, r)
            ),
            on_status=lambda s: self._on_status(s),
            on_error=lambda e: self._on_error(ten_env, e),
        )

        ten_env.log_info("[THYMIA] Sentinel client created, awaiting user info...")

        # Register tools
        tools = self.get_tool_metadata(ten_env)
        ten_env.log_info(f"[THYMIA] Registering {len(tools)} tools")

        await super().on_start(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        """Clean up on stop"""
        ten_env.log_info("[THYMIA] Stopping...")
        if self.sentinel_client:
            await self.sentinel_client.disconnect()
        await super().on_stop(ten_env)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Stream audio to Sentinel"""
        try:
            self._audio_frame_count += 1

            # Get PCM data from audio frame
            buf = audio_frame.lock_buf()
            pcm_data = bytes(buf)
            audio_frame.unlock_buf(buf)

            # Stream to Sentinel if connected
            if self.sentinel_client and self.sentinel_client.is_connected:
                await self.sentinel_client.send_audio(pcm_data, track="user")

                # Log periodically
                if self._audio_frame_count % 500 == 1:
                    status = self.sentinel_client.last_status
                    if status:
                        ten_env.log_info(
                            f"[THYMIA] Streaming: buffer={status.buffer_duration:.1f}s, "
                            f"speech={status.speech_duration:.1f}s, "
                            f"results={self.sentinel_results_count}"
                        )
            else:
                # Not connected - waiting for start_session
                if self._audio_frame_count % 1000 == 1:
                    ten_env.log_debug(
                        "[THYMIA] Waiting for Sentinel connection..."
                    )

        except Exception as e:
            ten_env.log_error(f"[THYMIA] Audio frame error: {e}")

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        """Forward transcripts to Sentinel if configured"""
        try:
            if not self.forward_transcripts:
                return

            if not self.sentinel_client or not self.sentinel_client.is_connected:
                return

            # Get text from data
            text = data.get_property_string("text")
            if not text:
                return

            # Determine speaker and finality
            is_final = data.get_property_bool("is_final")
            role = data.get_property_string("role") or "user"
            speaker = "agent" if role == "assistant" else "user"

            await self.sentinel_client.send_transcript(speaker, text, is_final)

        except Exception as e:
            ten_env.log_debug(f"[THYMIA] Transcript forward error: {e}")

    async def _connect(self, ten_env: AsyncTenEnv, session_id: str) -> bool:
        """Connect to Sentinel server"""
        if not self.sentinel_client:
            ten_env.log_error("[THYMIA] Sentinel client not initialized")
            return False

        if self.sentinel_client.is_connected:
            ten_env.log_debug("[THYMIA] Already connected")
            return True

        ten_env.log_info(
            f"[THYMIA] Connecting: user={self.user_name}, "
            f"dob={self.user_dob}, sex={self.user_sex}"
        )

        config = SentinelConfig(
            api_key=self.api_key,
            user_label=self.user_name or session_id,
            date_of_birth=self.user_dob or "1980-01-01",
            birth_sex=self.user_sex or "FEMALE",
            language=self.user_locale,
            biomarkers=self.biomarkers,
            policies=self.policies,
        )

        success = await self.sentinel_client.connect(config)
        if success:
            ten_env.log_info("[THYMIA] Connected to Sentinel server")
        else:
            ten_env.log_error("[THYMIA] Failed to connect to Sentinel")

        return success

    async def _on_policy_result(
        self, ten_env: AsyncTenEnv, result: PolicyResult
    ) -> None:
        """Handle PolicyResult from Sentinel"""
        self.sentinel_latest_result = result
        self.sentinel_results_count += 1

        ten_env.log_info(
            f"[THYMIA] Result #{self.sentinel_results_count}: "
            f"policy={result.policy}, type={result.analysis_type}"
        )

        # Map to backward-compatible formats
        wellness, apollo, safety = ResultMapper.extract_all(
            result,
            session_id=f"sentinel-{self.sentinel_results_count}",
        )

        # Update state
        if wellness:
            self.sentinel_wellness = wellness
            self.latest_results = WellnessMetrics(
                distress=wellness.distress,
                stress=wellness.stress,
                burnout=wellness.burnout,
                fatigue=wellness.fatigue,
                low_self_esteem=wellness.low_self_esteem,
                timestamp=wellness.timestamp,
                session_id=wellness.session_id,
            )
            ten_env.log_info(
                f"[THYMIA] Wellness: distress={wellness.distress:.0%}, "
                f"stress={wellness.stress:.0%}, burnout={wellness.burnout:.0%}"
            )

        if apollo:
            self.sentinel_apollo = apollo
            self.apollo_results = apollo
            ten_env.log_info(
                f"[THYMIA] Apollo: depression={apollo.depression_probability:.0%} "
                f"({apollo.depression_severity}), "
                f"anxiety={apollo.anxiety_probability:.0%} ({apollo.anxiety_severity})"
            )

        if safety:
            self.sentinel_safety = safety
            ten_env.log_info(
                f"[THYMIA] Safety: level={safety.level}, alert={safety.alert}"
            )

            # Handle high-risk classifications
            if safety.is_high_risk():
                ten_env.log_warn(f"[THYMIA] HIGH RISK: {safety.level}")
                await self._send_safety_alert(ten_env, safety)

        # Trigger LLM announcement
        if wellness or apollo:
            await self._send_results_alert(ten_env, result)

    def _on_status(self, status: StatusMessage) -> None:
        """Handle status update from Sentinel"""
        pass  # Status is available via sentinel_client.last_status

    def _on_error(self, ten_env: AsyncTenEnv, error: ErrorMessage) -> None:
        """Handle error from Sentinel"""
        ten_env.log_error(f"[THYMIA] Error: {error.error_code}: {error.message}")

    async def _send_results_alert(
        self, ten_env: AsyncTenEnv, result: PolicyResult
    ) -> None:
        """Send results announcement to LLM"""
        try:
            is_initial = result.analysis_type == "initial"

            if is_initial:
                hint = (
                    "[SYSTEM ALERT] Voice analysis complete. "
                    "Call get_wellness_metrics and share insights naturally."
                )
            else:
                hint = (
                    "[SYSTEM ALERT] Updated voice analysis available. "
                    "Call get_wellness_metrics if relevant to conversation."
                )

            ten_env.log_info(f"[THYMIA] Sending alert: {hint[:60]}...")

            text_data = Data.create("text_data")
            text_data.set_property_string("text", hint)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string("role", "system")
            await ten_env.send_data(text_data)

        except Exception as e:
            ten_env.log_error(f"[THYMIA] Alert error: {e}")

    async def _send_safety_alert(
        self, ten_env: AsyncTenEnv, safety: SafetyClassification
    ) -> None:
        """Send safety alert to LLM"""
        try:
            actions = ", ".join(safety.recommended_actions) if safety.recommended_actions else "none"
            hint = (
                f"[SAFETY ALERT] Risk level: {safety.level}. "
                f"Concerns: {safety.concerns}. "
                f"Recommended actions: {actions}. "
                f"Urgency: {safety.urgency}."
            )

            ten_env.log_warn(f"[THYMIA] Safety alert: {hint}")

            text_data = Data.create("text_data")
            text_data.set_property_string("text", hint)
            text_data.set_property_bool("end_of_segment", True)
            text_data.set_property_string("role", "system")
            await ten_env.send_data(text_data)

        except Exception as e:
            ten_env.log_error(f"[THYMIA] Safety alert error: {e}")

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        """Register LLM tools"""
        ten_env.log_info("[THYMIA] Registering tools")

        return [
            LLMToolMetadata(
                name="get_wellness_metrics",
                description=(
                    "Get user's voice analysis results. "
                    "Returns wellness metrics (stress, burnout, fatigue) and "
                    "clinical indicators (depression, anxiety) if available. "
                    "IMPORTANT: Call when you receive a [SYSTEM ALERT]. "
                    "Interpret results naturally - don't just list numbers. "
                    "If safety_classification shows high risk, follow recommended_actions."
                ),
                parameters=[],
            ),
            LLMToolMetadata(
                name="confirm_announcement",
                description=(
                    "REQUIRED: Call after announcing wellness results to user. "
                    "This confirms delivery and allows tracking."
                ),
                parameters=[
                    {
                        "name": "phase",
                        "type": "string",
                        "description": "Which results announced: 'hellos' or 'apollo'",
                        "required": True,
                    },
                ],
            ),
            LLMToolMetadata(
                name="start_session",
                description=(
                    "REQUIRED: Call once you have user's name, sex, and year of birth. "
                    "This starts voice analysis with their demographic info. "
                    "Analysis will not begin until this is called."
                ),
                parameters=[
                    {
                        "name": "name",
                        "type": "string",
                        "description": "User's first name",
                        "required": True,
                    },
                    {
                        "name": "year_of_birth",
                        "type": "string",
                        "description": "Year of birth (e.g. '1990')",
                        "required": True,
                    },
                    {
                        "name": "sex",
                        "type": "string",
                        "description": "MALE, FEMALE, or OTHER",
                        "required": True,
                    },
                    {
                        "name": "locale",
                        "type": "string",
                        "description": "Locale code (optional, e.g. en-GB)",
                        "required": False,
                    },
                ],
            ),
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult:
        """Handle LLM tool calls"""
        ten_env.log_info(f"[THYMIA] Tool call: {name}")

        try:
            if name == "get_wellness_metrics":
                return await self._handle_get_wellness_metrics(ten_env)

            elif name == "confirm_announcement":
                phase = args.get("phase", "")
                ten_env.log_info(f"[THYMIA] Announcement confirmed: {phase}")
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps({
                        "status": "confirmed",
                        "phase": phase,
                    }),
                )

            elif name == "start_session":
                return await self._handle_start_session(ten_env, args)

            else:
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps({"error": f"Unknown tool: {name}"}),
                )

        except Exception as e:
            ten_env.log_error(f"[THYMIA] Tool error: {e}")
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({"error": str(e)}),
            )

    async def _handle_get_wellness_metrics(
        self, ten_env: AsyncTenEnv
    ) -> LLMToolResult:
        """Handle get_wellness_metrics tool"""
        ten_env.log_info(
            f"[THYMIA] get_wellness_metrics: results={self.sentinel_results_count}, "
            f"wellness={self.sentinel_wellness is not None}, "
            f"apollo={self.sentinel_apollo is not None}"
        )

        # Get status from Sentinel client
        status_msg = self.sentinel_client.last_status if self.sentinel_client else None
        buffer_duration = status_msg.buffer_duration if status_msg else 0.0
        speech_duration = status_msg.speech_duration if status_msg else 0.0

        # Use ResultMapper to format response
        response_data = ResultMapper.format_tool_response(
            wellness=self.sentinel_wellness,
            apollo=self.sentinel_apollo,
            safety=self.sentinel_safety,
            analysis_mode="real_time",
            buffer_duration=buffer_duration,
            speech_duration=speech_duration,
            results_count=self.sentinel_results_count,
            analysis_type=(
                self.sentinel_latest_result.analysis_type
                if self.sentinel_latest_result
                else None
            ),
        )

        ten_env.log_info(f"[THYMIA] Response: {json.dumps(response_data)}")

        return LLMToolResultLLMResult(
            type="llmresult",
            content=json.dumps(response_data),
        )

    async def _handle_start_session(
        self, ten_env: AsyncTenEnv, args: dict
    ) -> LLMToolResult:
        """Handle start_session tool"""
        # Extract user info
        name = args.get("name", "")
        year_of_birth = args.get("year_of_birth", "")
        sex = args.get("sex", "").upper()
        locale = args.get("locale", "en-GB")

        # Validate
        if not name or not year_of_birth or not sex:
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "error",
                    "message": "Missing required fields: name, year_of_birth, sex",
                }),
            )

        # Normalize sex
        if sex == "OTHER":
            sex = "FEMALE"  # API only accepts MALE/FEMALE

        # Convert year to DOB
        try:
            year = int(year_of_birth)
            dob = f"{year}-01-01"
        except ValueError:
            dob = "1980-01-01"

        # Store user info
        self.user_name = name
        self.user_dob = dob
        self.user_sex = sex
        self.user_locale = locale

        ten_env.log_info(
            f"[THYMIA] Starting session: name={name}, dob={dob}, sex={sex}"
        )

        # Connect to Sentinel
        session_id = f"{name}-{int(time.time())}"
        success = await self._connect(ten_env, session_id)

        if success:
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "connected",
                    "message": f"Session started for {name}. Voice analysis is now active.",
                    "session_id": session_id,
                }),
            )
        else:
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps({
                    "status": "error",
                    "message": "Failed to connect to analysis server. Will retry.",
                }),
            )
