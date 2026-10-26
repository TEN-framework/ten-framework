import json
from typing import Any

from pydantic import BaseModel, Field
from ten_ai_base.utils import encrypt


DEFAULT_URL = "wss://dashscope.aliyuncs.com/api-ws/v1/inference"
MODEL_NAME = "qwen-audio-3.1-asr-flash-streaming"
SUPPORTED_LANGUAGES = {
    "zh",
    "en",
    "ja",
    "ko",
    "vi",
    "th",
    "id",
    "ms",
    "tl",
    "hi",
    "ar",
    "fr",
    "de",
    "es",
    "pt",
    "ru",
    "it",
    "nl",
    "sv",
    "da",
    "fi",
    "no",
    "el",
    "pl",
    "cs",
    "hu",
    "ro",
    "bg",
    "hr",
    "sk",
}


class QwenASRConfig(BaseModel):
    dump: bool = False
    dump_path: str = "/tmp"
    finalize_timeout_ms: int = 5000
    connect_timeout_s: int = 10
    reconnect_max_attempts: int = 5
    params: dict[str, Any] = Field(default_factory=dict)

    def apply_defaults(self) -> None:
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
            "language_hints": [],
        }
        normalized = dict(self.params or {})
        for key, value in defaults.items():
            normalized.setdefault(key, value)
        self.params = normalized

    def validate_config(self) -> None:
        params = self.params
        if not str(params.get("api_key", "")).strip():
            raise ValueError("Qwen API key is required")
        url = str(params.get("url", ""))
        if not url.startswith(("ws://", "wss://")):
            raise ValueError("Qwen WebSocket url must start with ws:// or wss://")
        if params.get("model") != MODEL_NAME:
            raise ValueError(f"model must be {MODEL_NAME}")
        if params.get("format") != "pcm":
            raise ValueError("Qwen ASR only accepts pcm input in this extension")
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
            raise ValueError("max_sentence_silence must be between 200 and 6000 ms")
        language_hints = params.get("language_hints") or []
        if not isinstance(language_hints, list) or len(language_hints) > 4:
            raise ValueError("language_hints must contain at most four languages")
        unsupported = set(language_hints) - SUPPORTED_LANGUAGES
        if unsupported:
            raise ValueError(f"unsupported language_hints: {sorted(unsupported)}")
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
                raise ValueError("vocabulary weights must be integers in [1, 5] or 50")
        if params.get("vocabulary_id") and vocabulary:
            raise ValueError("configure vocabulary_id or vocabulary, not both")
        if self.finalize_timeout_ms <= 0 or self.connect_timeout_s <= 0:
            raise ValueError("timeouts must be positive")
        if self.reconnect_max_attempts < 0:
            raise ValueError("reconnect_max_attempts cannot be negative")

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
        return json.dumps(value, ensure_ascii=False, sort_keys=True)

    @property
    def api_key(self) -> str:
        return str(self.params["api_key"])

    @property
    def url(self) -> str:
        return str(self.params["url"])

    @property
    def sample_rate(self) -> int:
        return int(self.params["sample_rate"])

    @property
    def language(self) -> str:
        hints = self.params.get("language_hints") or []
        return {
            "zh": "zh-CN",
            "en": "en-US",
            "ja": "ja-JP",
            "ko": "ko-KR",
        }.get(hints[0], hints[0] if hints else "und")

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
