#
# Thymia Sentinel WebSocket Protocol - Message Types
# Created by Claude Code in 2025.
#
"""
Protocol message types for the Thymia Sentinel WebSocket API.

This module defines all message types used in communication with the
Thymia Sentinel real-time analysis service at wss://ws.thymia.ai
"""

from dataclasses import dataclass, field
from typing import Optional, Literal, TypedDict
from enum import Enum

# ============================================================================
# Configuration Messages (Client → Server)
# ============================================================================


@dataclass
class SentinelConfig:
    """
    Configuration message sent when connecting to Sentinel WebSocket.

    This is the first message sent after WebSocket connection is established.
    """

    api_key: str
    user_label: str
    date_of_birth: str  # YYYY-MM-DD format
    birth_sex: str  # "MALE", "FEMALE", or "OTHER"
    language: str = "en-GB"
    buffer_strategy: str = "simple_reset"
    biomarkers: list[str] = field(default_factory=lambda: ["helios", "apollo"])
    policies: list[str] = field(
        default_factory=lambda: ["passthrough", "safety_analysis"]
    )
    sample_rate: int = 16000
    format: str = "pcm16"
    channels: int = 1

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "type": "CONFIG",
            "api_key": self.api_key,
            "user_label": self.user_label,
            "date_of_birth": self.date_of_birth,
            "birth_sex": self.birth_sex,
            "language": self.language,
            "buffer_strategy": self.buffer_strategy,
            "biomarkers": self.biomarkers,
            "policies": self.policies,
            "sample_rate": self.sample_rate,
            "format": self.format,
            "channels": self.channels,
        }


@dataclass
class AudioHeader:
    """
    Header message sent before each audio chunk.

    The server expects AUDIO_HEADER followed by raw audio bytes.
    """

    track: str  # "user" or "agent"
    format: str = "pcm16"
    sample_rate: int = 16000
    channels: int = 1
    bytes: int = 0  # Length of following audio data

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        return {
            "type": "AUDIO_HEADER",
            "track": self.track,
            "format": self.format,
            "sample_rate": self.sample_rate,
            "channels": self.channels,
            "bytes": self.bytes,
        }


@dataclass
class TranscriptMessage:
    """
    Transcript message for text content from user or agent.

    Enables concordance analysis between spoken content and biomarkers.
    """

    speaker: str  # "user" or "agent"
    text: str
    is_final: bool = True
    language: Optional[str] = None
    timestamp: Optional[float] = None

    def to_dict(self) -> dict:
        """Convert to dictionary for JSON serialization."""
        result = {
            "type": "TRANSCRIPT",
            "speaker": self.speaker,
            "text": self.text,
            "is_final": self.is_final,
        }
        if self.language:
            result["language"] = self.language
        if self.timestamp:
            result["timestamp"] = self.timestamp
        return result


# ============================================================================
# Server → Client Messages
# ============================================================================


class AlertLevel(str, Enum):
    """Safety alert levels from Sentinel analysis."""

    NONE = "none"
    MONITOR = "monitor"
    PROFESSIONAL_REFERRAL = "professional_referral"
    CRISIS = "crisis"


class Urgency(str, Enum):
    """Urgency levels for recommended actions."""

    ROUTINE = "routine"
    WITHIN_WEEK = "within_week"
    WITHIN_48HRS = "within_48hrs"
    WITHIN_24HRS = "within_24hrs"
    IMMEDIATE = "immediate"


class AnalysisType(str, Enum):
    """Types of analysis results."""

    INITIAL = "initial"
    UPDATE = "update"
    HOLISTIC = "holistic"


class Classification(TypedDict, total=False):
    """Safety classification from Sentinel analysis."""

    level: int  # Risk level 0-3
    alert: str  # "none", "monitor", "professional_referral", "crisis"
    confidence: str  # "low", "medium", "high"


class RecommendedActions(TypedDict, total=False):
    """Recommended actions from Sentinel analysis."""

    for_agent: str  # What the AI agent should do/say next
    for_human_reviewer: Optional[str]  # Notes for human reviewer
    urgency: (
        str  # "routine", "within_week", "within_48hrs", "within_24hrs", "immediate"
    )


class ConversationContext(TypedDict, total=False):
    """Context about the conversation analyzed."""

    mood_discussed: bool
    topics: list[str]
    user_insight: str  # "good", "fair", "poor", "unknown"


class ConcordanceAnalysis(TypedDict, total=False):
    """Analysis of text-biomarker concordance."""

    scenario: str  # "mood_not_discussed", "mood_discussed", "concordance", "minimization", "amplification"
    agreement_level: str  # "high", "moderate", "low", "n/a"
    mismatch_type: Optional[str]
    mismatch_severity: str  # "none", "mild", "moderate", "severe"


class SafetyFlags(TypedDict, total=False):
    """Safety flags from analysis."""

    suicidal_content: bool
    severe_mismatch: bool
    mood_not_yet_discussed: bool
    critical_symptoms: bool


@dataclass
class BiomarkerSummary:
    """
    Summary of all biomarker values from Sentinel analysis.

    Combines Helios wellness scores with Apollo disorder probabilities
    and individual symptom severities.
    """

    # Helios wellness scores (0.0 - 1.0)
    distress: Optional[float] = None
    stress: Optional[float] = None
    burnout: Optional[float] = None  # Also called exhaustion
    fatigue: Optional[float] = None  # Also called sleep_propensity
    low_self_esteem: Optional[float] = None
    mental_strain: Optional[float] = None

    # Emotion scores - real-time (0.0 - 1.0)
    neutral: Optional[float] = None
    happy: Optional[float] = None
    sad: Optional[float] = None
    angry: Optional[float] = None
    fearful: Optional[float] = None
    disgusted: Optional[float] = None
    surprised: Optional[float] = None

    # Apollo disorder probabilities (0.0 - 1.0)
    depression_probability: Optional[float] = None
    anxiety_probability: Optional[float] = None

    # Apollo depression symptom severities (0.0 - 1.0)
    symptom_anhedonia: Optional[float] = None
    symptom_low_mood: Optional[float] = None
    symptom_sleep_issues: Optional[float] = None
    symptom_low_energy: Optional[float] = None
    symptom_appetite: Optional[float] = None
    symptom_worthlessness: Optional[float] = None
    symptom_concentration: Optional[float] = None
    symptom_psychomotor: Optional[float] = None

    # Apollo anxiety symptom severities (0.0 - 1.0)
    symptom_nervousness: Optional[float] = None
    symptom_uncontrollable_worry: Optional[float] = None
    symptom_excessive_worry: Optional[float] = None
    symptom_trouble_relaxing: Optional[float] = None
    symptom_restlessness: Optional[float] = None
    symptom_irritability: Optional[float] = None
    symptom_dread: Optional[float] = None

    # Human-readable interpretation
    interpretation: Optional[str] = None

    @classmethod
    def from_dict(cls, data: dict) -> "BiomarkerSummary":
        """Create BiomarkerSummary from dictionary."""
        return cls(
            # Helios wellness scores
            distress=data.get("distress"),
            stress=data.get("stress"),
            burnout=data.get("burnout"),
            fatigue=data.get("fatigue"),
            low_self_esteem=data.get("low_self_esteem"),
            mental_strain=data.get("mental_strain"),
            # Emotion scores
            neutral=data.get("neutral"),
            happy=data.get("happy"),
            sad=data.get("sad"),
            angry=data.get("angry"),
            fearful=data.get("fearful"),
            disgusted=data.get("disgusted"),
            surprised=data.get("surprised"),
            # Apollo disorder probabilities
            depression_probability=data.get("depression_probability"),
            anxiety_probability=data.get("anxiety_probability"),
            # Depression symptoms
            symptom_anhedonia=data.get("symptom_anhedonia"),
            symptom_low_mood=data.get("symptom_low_mood"),
            symptom_sleep_issues=data.get("symptom_sleep_issues"),
            symptom_low_energy=data.get("symptom_low_energy"),
            symptom_appetite=data.get("symptom_appetite"),
            symptom_worthlessness=data.get("symptom_worthlessness"),
            symptom_concentration=data.get("symptom_concentration"),
            symptom_psychomotor=data.get("symptom_psychomotor"),
            # Anxiety symptoms
            symptom_nervousness=data.get("symptom_nervousness"),
            symptom_uncontrollable_worry=data.get("symptom_uncontrollable_worry"),
            symptom_excessive_worry=data.get("symptom_excessive_worry"),
            symptom_trouble_relaxing=data.get("symptom_trouble_relaxing"),
            symptom_restlessness=data.get("symptom_restlessness"),
            symptom_irritability=data.get("symptom_irritability"),
            symptom_dread=data.get("symptom_dread"),
            # Interpretation
            interpretation=data.get("interpretation"),
        )


@dataclass
class PolicyResult:
    """
    Result from a policy execution in Sentinel.

    This is the main result type received from the server, containing
    biomarkers, safety classification, concerns, and recommended actions.
    """

    policy: str  # Policy name (e.g., "passthrough", "safety_analysis")
    triggered_at_turn: int  # User turn that triggered this policy
    timestamp: float  # Unix timestamp

    # For safety_analysis policy
    analysis_type: Optional[str] = None  # "initial", "update", "holistic"
    segment_number: Optional[int] = None  # 1-indexed

    # Classification and concerns
    classification: Optional[Classification] = None
    concerns: Optional[list[str]] = None
    rationale: Optional[str] = None

    # Biomarker summary
    biomarker_summary: Optional[BiomarkerSummary] = None

    # Context and analysis
    conversation_context: Optional[ConversationContext] = None
    concordance_analysis: Optional[ConcordanceAnalysis] = None
    flags: Optional[SafetyFlags] = None

    # Actions
    recommended_actions: Optional[RecommendedActions] = None

    @classmethod
    def from_dict(cls, data: dict) -> "PolicyResult":
        """Create PolicyResult from dictionary received from server."""
        result = data.get("result", {})

        # Parse biomarker summary if present
        biomarker_summary = None
        raw_biomarkers = result.get("biomarker_summary")
        if raw_biomarkers:
            if isinstance(raw_biomarkers, dict):
                biomarker_summary = BiomarkerSummary.from_dict(raw_biomarkers)
            elif hasattr(raw_biomarkers, "model_dump"):
                biomarker_summary = BiomarkerSummary.from_dict(
                    raw_biomarkers.model_dump()
                )

        return cls(
            policy=data.get("policy", "unknown"),
            triggered_at_turn=data.get("triggered_at_turn", 0),
            timestamp=data.get("timestamp", 0.0),
            analysis_type=result.get("analysis_type"),
            segment_number=result.get("segment_number"),
            classification=result.get("classification"),
            concerns=result.get("concerns"),
            rationale=result.get("rationale"),
            biomarker_summary=biomarker_summary,
            conversation_context=result.get("conversation_context"),
            concordance_analysis=result.get("concordance_analysis"),
            flags=result.get("flags"),
            recommended_actions=result.get("recommended_actions"),
        )


@dataclass
class StatusMessage:
    """
    Status update from Sentinel server about buffer state.

    Received periodically to track audio buffering progress.
    """

    buffer_duration: float  # Total buffered duration in seconds
    speech_duration: float  # Detected speech duration in seconds

    @classmethod
    def from_dict(cls, data: dict) -> "StatusMessage":
        """Create StatusMessage from dictionary."""
        return cls(
            buffer_duration=data.get("buffer_duration", 0.0),
            speech_duration=data.get("speech_duration", 0.0),
        )


@dataclass
class ErrorMessage:
    """
    Error message from Sentinel server.
    """

    error_code: str
    message: str
    details: Optional[str] = None

    @classmethod
    def from_dict(cls, data: dict) -> "ErrorMessage":
        """Create ErrorMessage from dictionary."""
        return cls(
            error_code=data.get("error_code", "UNKNOWN"),
            message=data.get("message", "Unknown error"),
            details=data.get("details"),
        )
