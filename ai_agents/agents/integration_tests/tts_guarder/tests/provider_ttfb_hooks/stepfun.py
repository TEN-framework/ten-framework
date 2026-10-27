"""StepFun text delta and WAV audio delta, ignoring session/control events."""

import base64
import json
from .websocket import WebSocketHook
from .oracle import AudioDetector


class Hook(WebSocketHook):
    module_name = "ten_packages.extension.stepfun_tts_python.stepfun_tts"
    default_url = "wss://api.stepfun.com/v1/realtime/audio"

    @staticmethod
    def is_text(raw, request_id):
        message = json.loads(raw)
        return message.get("type") == "tts.text.delta" and bool(
            (message.get("data") or {}).get("text", "").strip()
        )

    @staticmethod
    def is_audio(raw, request_id):
        message = json.loads(raw)
        audio = (message.get("data") or {}).get("audio")
        return message.get("type") == "tts.response.audio.delta" and bool(
            audio and AudioDetector().feed(base64.b64decode(audio, validate=True))
        )
