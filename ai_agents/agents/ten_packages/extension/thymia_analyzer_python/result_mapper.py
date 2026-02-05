#
# Thymia Sentinel Result Mapper - Backward Compatibility
# Created by Claude Code in 2025.
#
"""
Maps Sentinel PolicyResult to existing WellnessMetrics and ApolloResult formats.

This module ensures backward compatibility with the existing REST API result
formats when using the new Sentinel WebSocket API.
"""

import time
from typing import Optional, Tuple

from .sentinel_protocol import (
    PolicyResult,
    BiomarkerSummary,
    Classification,
    RecommendedActions,
)

# Import existing types if available
try:
    from .apollo_api import ApolloResult
except ImportError:
    ApolloResult = None


class WellnessMetricsCompat:
    """
    Backward-compatible WellnessMetrics class.

    Matches the existing WellnessMetrics dataclass structure.
    """

    def __init__(
        self,
        distress: float = 0.0,
        stress: float = 0.0,
        burnout: float = 0.0,
        fatigue: float = 0.0,
        low_self_esteem: float = 0.0,
        timestamp: float = 0.0,
        session_id: str = "",
    ):
        self.distress = distress
        self.stress = stress
        self.burnout = burnout
        self.fatigue = fatigue
        self.low_self_esteem = low_self_esteem
        self.timestamp = timestamp
        self.session_id = session_id


class SafetyClassification:
    """
    Safety classification result from Sentinel analysis.

    This is a new type not available in the REST API.
    """

    def __init__(
        self,
        level: int = 0,
        alert: str = "none",
        confidence: str = "medium",
        concerns: Optional[list[str]] = None,
        recommended_actions: Optional[dict] = None,
        urgency: str = "routine",
    ):
        self.level = level
        self.alert = alert
        self.confidence = confidence
        self.concerns = concerns or []
        self.recommended_actions = recommended_actions or {}
        self.urgency = urgency

    def is_high_risk(self) -> bool:
        """Check if this represents a high-risk classification."""
        return self.level >= 2 or self.alert in ("professional_referral", "crisis")

    def requires_immediate_action(self) -> bool:
        """Check if immediate action is required."""
        return self.alert == "crisis" or self.urgency == "immediate"


class ResultMapper:
    """
    Maps Sentinel PolicyResult to backward-compatible formats.
    """

    @staticmethod
    def to_wellness_metrics(
        result: PolicyResult,
        session_id: str = "",
    ) -> Optional[WellnessMetricsCompat]:
        """
        Convert PolicyResult to WellnessMetrics format.

        Args:
            result: PolicyResult from Sentinel
            session_id: Session identifier for backward compatibility

        Returns:
            WellnessMetricsCompat or None if no biomarkers available
        """
        if not result.biomarker_summary:
            return None

        bio = result.biomarker_summary

        # Check if we have any Helios metrics
        if all(
            v is None
            for v in [
                bio.distress,
                bio.stress,
                bio.burnout,
                bio.fatigue,
                bio.low_self_esteem,
            ]
        ):
            return None

        return WellnessMetricsCompat(
            distress=bio.distress or 0.0,
            stress=bio.stress or 0.0,
            burnout=bio.burnout or 0.0,
            fatigue=bio.fatigue or 0.0,
            low_self_esteem=bio.low_self_esteem or 0.0,
            timestamp=result.timestamp or time.time(),
            session_id=session_id,
        )

    @staticmethod
    def to_apollo_result(result: PolicyResult) -> Optional["ApolloResult"]:
        """
        Convert PolicyResult to ApolloResult format.

        Args:
            result: PolicyResult from Sentinel

        Returns:
            ApolloResult or None if no Apollo biomarkers available
        """
        if ApolloResult is None:
            return None

        if not result.biomarker_summary:
            return None

        bio = result.biomarker_summary

        # Check if we have Apollo metrics
        if bio.depression_probability is None and bio.anxiety_probability is None:
            return None

        # Map severity from probability
        def probability_to_severity(prob: Optional[float]) -> Optional[str]:
            if prob is None:
                return None
            if prob < 0.25:
                return "NONE"
            elif prob < 0.50:
                return "MILD"
            elif prob < 0.75:
                return "MODERATE"
            else:
                return "SEVERE"

        return ApolloResult(
            status="COMPLETE_OK",
            depression_probability=bio.depression_probability,
            depression_severity=probability_to_severity(bio.depression_probability),
            anxiety_probability=bio.anxiety_probability,
            anxiety_severity=probability_to_severity(bio.anxiety_probability),
        )

    @staticmethod
    def to_safety_classification(
        result: PolicyResult,
    ) -> Optional[SafetyClassification]:
        """
        Extract SafetyClassification from PolicyResult.

        Args:
            result: PolicyResult from Sentinel

        Returns:
            SafetyClassification or None if not available
        """
        if not result.classification:
            return None

        classification = result.classification
        actions = result.recommended_actions or {}

        return SafetyClassification(
            level=classification.get("level", 0),
            alert=classification.get("alert", "none"),
            confidence=classification.get("confidence", "medium"),
            concerns=result.concerns,
            recommended_actions=actions,
            urgency=actions.get("urgency", "routine"),
        )

    @staticmethod
    def extract_all(
        result: PolicyResult,
        session_id: str = "",
    ) -> Tuple[
        Optional[WellnessMetricsCompat],
        Optional["ApolloResult"],
        Optional[SafetyClassification],
    ]:
        """
        Extract all result types from a PolicyResult.

        Args:
            result: PolicyResult from Sentinel
            session_id: Session identifier for backward compatibility

        Returns:
            Tuple of (WellnessMetrics, ApolloResult, SafetyClassification)
        """
        wellness = ResultMapper.to_wellness_metrics(result, session_id)
        apollo = ResultMapper.to_apollo_result(result)
        safety = ResultMapper.to_safety_classification(result)

        return wellness, apollo, safety

    @staticmethod
    def format_tool_response(
        wellness: Optional[WellnessMetricsCompat],
        apollo: Optional["ApolloResult"],
        safety: Optional[SafetyClassification],
        analysis_mode: str = "real_time",
        buffer_duration: float = 0.0,
        speech_duration: float = 0.0,
        results_count: int = 0,
        analysis_type: Optional[str] = None,
    ) -> dict:
        """
        Format results for LLM tool response (get_wellness_metrics).

        Args:
            wellness: WellnessMetrics or None
            apollo: ApolloResult or None
            safety: SafetyClassification or None
            analysis_mode: "real_time" for sentinel, "batch" for REST
            buffer_duration: Server-reported buffer duration
            speech_duration: Server-reported speech duration
            results_count: Number of results received
            analysis_type: "initial", "update", or "holistic"

        Returns:
            Dictionary suitable for JSON response to LLM
        """
        # Determine status
        has_wellness = wellness is not None
        has_apollo = apollo is not None and apollo.status == "COMPLETE_OK"

        if not has_wellness and not has_apollo:
            return {
                "status": "streaming",
                "message": "Real-time analysis in progress. Results will be available soon.",
                "buffer_duration_seconds": round(buffer_duration, 1),
                "speech_duration_seconds": round(speech_duration, 1),
                "results_received": results_count > 0,
                "analysis_mode": analysis_mode,
            }

        # Build response
        response = {
            "status": "available",
            "analysis_mode": analysis_mode,
        }

        if analysis_type:
            response["analysis_type"] = analysis_type

        # Add wellness metrics (key 3: stress, burnout, fatigue)
        if has_wellness:
            response["metrics"] = {
                "stress": round(wellness.stress * 100),
                "burnout": round(wellness.burnout * 100),
                "fatigue": round(wellness.fatigue * 100),
            }

        # Add clinical indicators if available
        if has_apollo:
            response["clinical_indicators"] = {
                "depression": {
                    "probability": round(apollo.depression_probability * 100),
                    "severity": apollo.depression_severity,
                },
                "anxiety": {
                    "probability": round(apollo.anxiety_probability * 100),
                    "severity": apollo.anxiety_severity,
                },
            }

        # Add safety classification if available
        if safety:
            response["safety_classification"] = {
                "level": ["low", "medium", "high", "critical"][min(safety.level, 3)],
                "alert": safety.alert != "none",
                "concerns": safety.concerns,
                "recommended_actions": safety.recommended_actions.get("for_agent", []),
                "urgency": safety.urgency,
            }

        return response

    @staticmethod
    def format_phase_progress_response(
        buffer_duration: float,
        speech_duration: float,
        results_received: bool,
        analysis_type: Optional[str] = None,
        is_connected: bool = True,
    ) -> dict:
        """
        Format response for check_phase_progress tool in Sentinel mode.

        No phases in Sentinel mode - server handles all buffering and
        decides when to analyze.

        Args:
            buffer_duration: Server-reported buffer duration
            speech_duration: Server-reported speech duration
            results_received: Whether any results have been received
            analysis_type: Type of most recent analysis
            is_connected: Whether connected to Sentinel server

        Returns:
            Dictionary suitable for JSON response to LLM
        """
        if not is_connected:
            return {
                "status": "disconnected",
                "message": "Not connected to analysis server. Attempting reconnect.",
            }

        return {
            "status": "streaming",
            "buffer_duration_seconds": round(buffer_duration, 1),
            "speech_duration_seconds": round(speech_duration, 1),
            "results_received": results_received,
            "analysis_type": analysis_type,
            "message": (
                f"Streaming audio to analysis server. "
                f"Buffer: {buffer_duration:.1f}s, Speech: {speech_duration:.1f}s. "
                f"Server will push results when ready."
            ),
        }
