import re
from typing import Any, Dict, List, Optional
from pydantic import BaseModel, ConfigDict, Field
from ten_ai_base.utils import redact_json, redact_url

from .const import (
    AGENT_CONTEXT_MAX_CHARS,
    DEFAULT_FINALIZE_TIMEOUT_MS,
    MAX_KEYTERMS,
    MAX_LANGUAGE_CODES,
    PROMPT_MAX_CHARS,
    VALID_ENCODINGS,
    VALID_MODES,
    VALID_PII_SUBSTITUTIONS,
    VALID_VOICE_FOCUS,
)

# Models in the Universal-3 Pro family (universal-3-6-pro, universal-3-5-pro,
# future universal-3-N-pro releases, and the legacy u3-rt-pro builds) share
# the same parameter surface: prompting, conversational context, language
# steering, voice focus and the `mode` preset. Universal Streaming
# (english/multilingual) models do not.
_PRO_MODEL_PATTERN = re.compile(
    r"^(universal-\d+(-\d+)*-pro|u3-rt-pro(-[a-z0-9]+)*)$"
)

# Connection parameters that only the Pro family honours.
PRO_ONLY_PARAMS = frozenset(
    {
        "mode",
        "prompt",
        "agent_context",
        "previous_context_n_turns",
        "language_codes",
        "voice_focus",
        "voice_focus_threshold",
        "interruption_delay",
        "continuous_partials",
    }
)

# Legacy Universal Streaming turn-detection knobs. The Pro family uses
# punctuation-based turn detection and ignores these.
LEGACY_STREAMING_PARAMS = frozenset(
    {
        "end_of_turn_confidence_threshold",
        "min_end_of_turn_silence_when_confident",
    }
)

# Fields that configure the extension itself and must never reach the vendor
# query string.
_LOCAL_FIELDS = frozenset(
    {
        "api_key",
        "ws_url",
        "language",
        "finalize_timeout_ms",
        "dump",
        "dump_path",
        "params",
        "extra_params",
    }
)

# ISO 639-1 code -> locale reported in `asr_result.language`.
_LANGUAGE_LOCALE_MAP = {
    "zh": "zh-CN",
    "en": "en-US",
    "ja": "ja-JP",
    "ko": "ko-KR",
    "de": "de-DE",
    "fr": "fr-FR",
    "ru": "ru-RU",
    "es": "es-ES",
    "pt": "pt-PT",
    "it": "it-IT",
}


class AssemblyAIASRConfig(BaseModel):
    """AssemblyAI Streaming (v3) ASR configuration.

    All vendor knobs live under ``params`` in property.json; ``update`` copies
    known keys onto the typed fields (coercing scalar types) and keeps unknown
    keys in ``extra_params`` so they can be forwarded verbatim to the API.
    """

    model_config = ConfigDict(validate_assignment=True)

    # Authentication / endpoint
    api_key: str = ""
    ws_url: str = "wss://streaming.assemblyai.com/v3/ws"

    # Model and audio
    speech_model: str = "universal-3-6-pro"
    sample_rate: int = 16000
    encoding: str = "pcm_s16le"
    format_turns: bool = True

    # Language: `language` is the locale reported downstream (and used to
    # derive `language_codes` when that is not set explicitly).
    language: str = "en-US"
    language_codes: Optional[List[str]] = None
    language_detection: Optional[bool] = None

    # Prompting and conversational context (Pro family)
    mode: Optional[str] = None
    prompt: Optional[str] = None
    keyterms_prompt: List[str] = Field(default_factory=list)
    agent_context: Optional[str] = None
    previous_context_n_turns: Optional[int] = None

    # Turn detection tuning
    min_turn_silence: Optional[int] = None
    max_turn_silence: Optional[int] = None
    vad_threshold: Optional[float] = None
    interruption_delay: Optional[int] = None
    continuous_partials: Optional[bool] = None
    include_partial_turns: Optional[bool] = None

    # Legacy Universal Streaming turn detection
    end_of_turn_confidence_threshold: Optional[float] = None
    min_end_of_turn_silence_when_confident: Optional[int] = None

    # Audio front-end and diarization
    voice_focus: Optional[str] = None
    voice_focus_threshold: Optional[float] = None
    speaker_labels: Optional[bool] = None
    max_speakers: Optional[int] = None

    # Domain, redaction, session
    domain: Optional[str] = None
    redact_pii: Optional[bool] = None
    redact_pii_policies: Optional[List[str]] = None
    redact_pii_sub: Optional[str] = None
    filter_profanity: Optional[bool] = None
    inactivity_timeout: Optional[int] = None
    session_heartbeat: Optional[bool] = None

    # Extension behaviour (not sent to the vendor)
    finalize_timeout_ms: int = DEFAULT_FINALIZE_TIMEOUT_MS

    # Debugging and dumping
    dump: bool = False
    dump_path: str = "/tmp"

    # Raw params from property.json and anything we do not know about.
    params: Dict[str, Any] = Field(default_factory=dict)
    extra_params: Dict[str, Any] = Field(default_factory=dict)

    def update(self, params: Dict[str, Any]) -> None:
        """Copy ``params`` onto typed fields; keep unknown keys for passthrough."""
        known = set(type(self).model_fields) - {"params", "extra_params"}
        for key, value in params.items():
            if key in known:
                setattr(self, key, value)
            else:
                self.extra_params[key] = value

    def validate_config(self) -> None:
        """Raise ``ValueError`` describing every invalid setting."""
        problems: List[str] = []

        if not self.api_key or not self.api_key.strip():
            problems.append("api_key is required")
        if not self.ws_url.startswith(("ws://", "wss://")):
            problems.append(
                f"ws_url must be a ws(s):// URL, got {self.ws_url!r}"
            )
        if self.sample_rate <= 0:
            problems.append("sample_rate must be a positive integer")
        if self.finalize_timeout_ms <= 0:
            problems.append("finalize_timeout_ms must be a positive integer")

        for name, allowed in (
            ("encoding", VALID_ENCODINGS),
            ("mode", VALID_MODES),
            ("voice_focus", VALID_VOICE_FOCUS),
            ("redact_pii_sub", VALID_PII_SUBSTITUTIONS),
        ):
            value = getattr(self, name)
            if value is not None and value not in allowed:
                problems.append(
                    f"{name} must be one of {sorted(allowed)}, got {value!r}"
                )

        for name, low, high in (
            ("vad_threshold", 0.0, 1.0),
            ("voice_focus_threshold", 0.0, 1.0),
            ("interruption_delay", 0, 1000),
            ("previous_context_n_turns", 0, 100),
            ("max_speakers", 1, 10),
        ):
            value = getattr(self, name)
            if value is not None and not low <= value <= high:
                problems.append(f"{name} must be within [{low}, {high}]")

        for name, limit in (
            ("prompt", PROMPT_MAX_CHARS),
            ("agent_context", AGENT_CONTEXT_MAX_CHARS),
        ):
            value = getattr(self, name)
            if value is not None and len(value) > limit:
                problems.append(f"{name} must be at most {limit} characters")

        if len(self.keyterms_prompt) > MAX_KEYTERMS:
            problems.append(
                f"keyterms_prompt allows at most {MAX_KEYTERMS} terms"
            )
        if (
            self.language_codes is not None
            and len(self.language_codes) > MAX_LANGUAGE_CODES
        ):
            problems.append(
                f"language_codes allows at most {MAX_LANGUAGE_CODES} codes"
            )

        if problems:
            raise ValueError("; ".join(problems))

    @property
    def is_pro_model(self) -> bool:
        return bool(_PRO_MODEL_PATTERN.match(self.speech_model))

    @property
    def normalized_language(self) -> str:
        """Locale reported in asr_result when no per-turn language is known."""
        return _LANGUAGE_LOCALE_MAP.get(self.language, self.language)

    def language_for_code(self, code: Optional[str]) -> str:
        """Map a detected ISO 639-1 code to a locale, falling back sensibly."""
        if not code:
            return self.normalized_language
        return _LANGUAGE_LOCALE_MAP.get(code.lower(), code)

    def effective_language_codes(self) -> List[str]:
        """Explicit ``language_codes`` win; otherwise derive from ``language``.

        No steering is derived when ``language_detection`` is on, so the model
        code-switches freely and reports what it heard.
        """
        if self.language_codes is not None:
            return list(self.language_codes)
        if self.language_detection or not self.language:
            return []
        return [re.split(r"[-_]", self.language)[0].lower()]

    def dropped_connection_params(self) -> List[str]:
        """Configured params that the selected model does not accept."""
        gated = (
            LEGACY_STREAMING_PARAMS if self.is_pro_model else PRO_ONLY_PARAMS
        )
        values = self.model_dump(exclude_none=True)
        return sorted(
            key
            for key in gated
            if key in values and values[key] not in ([], "")
        )

    def to_connection_params(self) -> Dict[str, Any]:
        """Build the vendor connection parameters for the current model.

        Pro-only parameters are dropped for Universal Streaming models and
        legacy turn-detection parameters are dropped for Pro models; unknown
        ``extra_params`` are forwarded verbatim.
        """
        values = self.model_dump(exclude=set(_LOCAL_FIELDS), exclude_none=True)
        params: Dict[str, Any] = {}
        for key, value in values.items():
            if key == "language_codes":
                continue
            if key in PRO_ONLY_PARAMS and not self.is_pro_model:
                continue
            if key in LEGACY_STREAMING_PARAMS and self.is_pro_model:
                continue
            if key == "keyterms_prompt" and not value:
                continue
            params[key] = value

        language_codes = self.effective_language_codes()
        if self.is_pro_model and language_codes:
            params["language_codes"] = language_codes

        params.update(self.extra_params)
        return params

    def to_redacted_dict(self) -> Dict[str, Any]:
        """Config snapshot safe for logs: secret-looking keys anywhere in
        the tree (including ``params`` / ``extra_params``) and signed query
        values in ``ws_url`` are masked."""
        config_dict = redact_json(self.model_dump())
        for key in ("ws_url",):
            if isinstance(config_dict.get(key), str):
                config_dict[key] = redact_url(config_dict[key])
        for section in ("params", "extra_params"):
            value = config_dict.get(section)
            if isinstance(value, dict) and isinstance(value.get("ws_url"), str):
                value["ws_url"] = redact_url(value["ws_url"])
        return config_dict

    def to_json(self, sensitive_handling: bool = False) -> str:
        """Serialize for logging, masking secrets when requested."""
        if sensitive_handling:
            return str(self.to_redacted_dict())
        return str(self.model_dump())
