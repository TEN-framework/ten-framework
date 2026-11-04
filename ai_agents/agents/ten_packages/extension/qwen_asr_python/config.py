import json
from typing import Any
from urllib.parse import urlsplit

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt, redact_url


DEFAULT_URL = "wss://dashscope.aliyuncs.com/api-ws/v1/inference"
MODEL_NAME = "qwen-audio-3.1-asr-flash-streaming"
# Qwen language_hints (ISO 639-1) -> TEN asr_result.language (BCP-47).
# See https://help.aliyun.com/zh/model-studio/asr-model
LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE: dict[str, str] = {
    "zh": "zh-CN",
    "en": "en-US",
    "ja": "ja-JP",
    "ko": "ko-KR",
    "vi": "vi-VN",
    "th": "th-TH",
    "id": "id-ID",
    "ms": "ms-MY",
    "tl": "tl-PH",
    "hi": "hi-IN",
    "ar": "ar-SA",
    "fr": "fr-FR",
    "de": "de-DE",
    "es": "es-ES",
    "pt": "pt-PT",
    "ru": "ru-RU",
    "it": "it-IT",
    "nl": "nl-NL",
    "sv": "sv-SE",
    "da": "da-DK",
    "fi": "fi-FI",
    "no": "nb-NO",
    "el": "el-GR",
    "pl": "pl-PL",
    "cs": "cs-CZ",
    "hu": "hu-HU",
    "ro": "ro-RO",
    "bg": "bg-BG",
    "hr": "hr-HR",
    "sk": "sk-SK",
}
SUPPORTED_LANGUAGES = set(LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE)
ASR_RESULT_LANGUAGE_TO_HINT: dict[str, str] = {
    bcp47: hint for hint, bcp47 in LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE.items()
}


class QwenASRConfig(BaseModel):
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_timeout_ms: int = 5000
    connect_timeout_s: int = 10
    language: str = "zh-CN"
    params: dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None:
        normalized = dict(self.params or {})
        defaults = {
            "api_key": "",
            "url": DEFAULT_URL,
            "model": MODEL_NAME,
            "format": "pcm",
            "sample_rate": 16000,
            "vad_model": "near_meeting_16k",
            "max_sentence_silence": 240,
            "keep_dialect": False,
            "heartbeat": False,
        }
        for key, value in defaults.items():
            normalized.setdefault(key, value)
        self.params = normalized
        self._sync_language_hints_from_language()

    def validate_config(self) -> None:
        params = self.params
        if not str(params.get("api_key", "")).strip():
            raise ValueError("Qwen API key is required")
        url = str(params.get("url", ""))
        parsed_url = urlsplit(url)
        try:
            parsed_port = parsed_url.port
        except ValueError as exc:
            raise ValueError("Qwen WebSocket url has an invalid port") from exc
        if (
            parsed_url.scheme not in {"ws", "wss"}
            or not parsed_url.hostname
            or parsed_url.username is not None
            or parsed_url.password is not None
            or parsed_port is not None
            and not 1 <= parsed_port <= 65535
        ):
            raise ValueError(
                "Qwen WebSocket url must include a valid ws:// or wss:// host "
                "without credentials"
            )
        if params.get("model") != MODEL_NAME:
            raise ValueError(f"model must be {MODEL_NAME}")
        if params.get("format") != "pcm":
            raise ValueError(
                "Qwen ASR only accepts pcm input in this extension"
            )
        sample_rate = params.get("sample_rate")
        if not isinstance(sample_rate, int) or sample_rate <= 0:
            raise ValueError("sample_rate must be a positive integer")
        if params.get("vad_model") not in {
            "near_meeting_16k",
            "far_field_meeting_16k",
        }:
            raise ValueError(
                "vad_model must be near_meeting_16k or far_field_meeting_16k"
            )
        silence = params.get("max_sentence_silence")
        if not isinstance(silence, int) or not 200 <= silence <= 6000:
            raise ValueError(
                "max_sentence_silence must be between 200 and 6000 ms"
            )
        language_hints = params.get("language_hints") or []
        if not isinstance(language_hints, list) or len(language_hints) > 4:
            raise ValueError(
                "language_hints must contain at most four languages"
            )
        unsupported = set(language_hints) - SUPPORTED_LANGUAGES
        if unsupported:
            raise ValueError(
                f"unsupported language_hints: {sorted(unsupported)}"
            )
        vocabulary = params.get("vocabulary")
        if vocabulary is not None:
            if not isinstance(vocabulary, dict):
                raise ValueError("vocabulary must be an object")
            if len(vocabulary) > 50:
                raise ValueError("vocabulary may contain at most 50 entries")
            if any(
                not isinstance(word, str)
                or not word.strip()
                or weight not in {1, 2, 3, 4, 5, 50}
                for word, weight in vocabulary.items()
            ):
                raise ValueError(
                    "vocabulary weights must be integers in [1, 5] or 50"
                )
        if params.get("vocabulary_id") and vocabulary:
            raise ValueError("configure vocabulary_id or vocabulary, not both")
        if self.finalize_timeout_ms <= 0 or self.connect_timeout_s <= 0:
            raise ValueError("timeouts must be positive")

    def to_json(self, sensitive_handling: bool = False) -> str:
        value = self.model_dump()
        if sensitive_handling:
            params = value.get("params", {})
            for key in list(params):
                if any(
                    token in key.lower()
                    for token in ("key", "token", "secret", "password")
                ):
                    params[key] = encrypt(str(params[key]))
            if "url" in params:
                params["url"] = redact_url(str(params["url"]))
        return json.dumps(value, ensure_ascii=False, sort_keys=True)

    @property
    def api_key(self) -> str:
        return str(self.params["api_key"])

    @property
    def url(self) -> str:
        return str(self.params["url"])

    @property
    def redacted_url(self) -> str:
        return redact_url(self.url)

    @property
    def sample_rate(self) -> int:
        return int(self.params["sample_rate"])

    @staticmethod
    def _map_language_code_to_bcp47(language_code: str) -> str:
        code = language_code.strip()
        if not code:
            return "zh-CN"
        if "-" in code:
            return code
        return LANGUAGE_HINT_TO_ASR_RESULT_LANGUAGE.get(code, code)

    def _language_hint_from_configured_language(self) -> str | None:
        raw = str(self.language or "").strip()
        if not raw:
            return None
        bcp47 = self._map_language_code_to_bcp47(raw)
        hint = ASR_RESULT_LANGUAGE_TO_HINT.get(bcp47)
        if hint is not None:
            return hint
        if bcp47 in SUPPORTED_LANGUAGES:
            return bcp47
        return None

    def _sync_language_hints_from_language(self) -> None:
        """Fill language_hints from property.language when params has none."""
        explicit = self.params.get("language_hints")
        if isinstance(explicit, list) and len(explicit) > 0:
            return
        hint = self._language_hint_from_configured_language()
        self.params["language_hints"] = [hint] if hint else []

    @property
    def normalized_language(self) -> str:
        """TEN-facing asr.language (property.language), normalized to BCP-47."""
        return self._map_language_code_to_bcp47(str(self.language or "zh-CN"))

    @staticmethod
    def asr_result_language_from_vendor(
        payload: dict[str, Any],
        output: dict[str, Any],
        sentence: dict[str, Any],
    ) -> str | None:
        """Map vendor-reported language or language_hints to BCP-47."""
        for container in (sentence, output, payload):
            for key in ("language", "lang"):
                value = container.get(key)
                if isinstance(value, str) and value.strip():
                    return QwenASRConfig._map_language_code_to_bcp47(value)
            hints = container.get("language_hints")
            if isinstance(hints, list) and hints:
                first = hints[0]
                if isinstance(first, str) and first.strip():
                    return QwenASRConfig._map_language_code_to_bcp47(first)
        return None

    def build_run_task(self, task_id: str) -> dict[str, Any]:
        params = self.params
        recognition_parameters: dict[str, Any] = {
            "format": params["format"],
            "sample_rate": params["sample_rate"],
            "vad_model": params["vad_model"],
            "max_sentence_silence": params["max_sentence_silence"],
            "keep_dialect": params["keep_dialect"],
            "heartbeat": params["heartbeat"],
        }
        for key in (
            "vocabulary_id",
            "vocabulary",
            "language_hints",
            "context",
            "enable_multi_threshold_mode",
            "speech_noise_threshold",
        ):
            value = params.get(key)
            if value not in (None, "", [], {}):
                recognition_parameters[key] = value
        payload: dict[str, Any] = {
            "task_group": "audio",
            "task": "asr",
            "function": "recognition",
            "model": params["model"],
            "parameters": recognition_parameters,
            "input": {},
        }
        if params.get("context"):
            payload["input"] = {"context": params["context"]}
        return {
            "header": {
                "action": "run-task",
                "task_id": task_id,
                "streaming": "duplex",
            },
            "payload": payload,
        }

    @staticmethod
    def build_finish_task(task_id: str) -> dict[str, Any]:
        return {
            "header": {
                "action": "finish-task",
                "task_id": task_id,
                "streaming": "duplex",
            },
            "payload": {"input": {}},
        }
