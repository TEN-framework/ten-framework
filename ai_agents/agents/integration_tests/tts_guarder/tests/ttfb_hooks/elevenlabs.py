"""Observe TTS and TTD context IDs and nonempty decoded provider audio."""

import base64
import json

from .base import ConnectionObserver


class ElevenLabsObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        message = json.loads(raw)
        text = message.get("text", "")
        if message.get("inputs"):
            text = "".join(item.get("text", "") for item in message["inputs"])
        if text.strip() and not message.get("close_context"):
            return message.get("context_id")
        return None

    def audio_context(self, raw):
        message = json.loads(raw)
        audio = message.get("audio")
        if audio and base64.b64decode(audio, validate=True):
            return message.get("context_id") or message.get("contextId")
        return None
