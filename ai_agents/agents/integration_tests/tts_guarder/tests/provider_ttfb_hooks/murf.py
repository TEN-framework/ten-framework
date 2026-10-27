"""Murf text and base64 audio frames; control/config messages are excluded."""

import base64
import json
from .websocket import WebSocketHook


class Hook(WebSocketHook):
    module_name = "ten_packages.extension.murf_tts_python.murf_tts"
    default_url = "wss://global.api.murf.ai/v1/speech/stream-input"

    @staticmethod
    def is_text(raw, request_id):
        message = json.loads(raw)
        return bool(
            message.get("text", "").strip() and message.get("context_id") == request_id
        )

    @staticmethod
    def is_audio(raw, request_id):
        message = json.loads(raw)
        audio = message.get("audio")
        return bool(
            message.get("context_id") == request_id
            and audio
            and base64.b64decode(audio, validate=True)
        )
