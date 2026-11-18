"""Kalpa sendText text and responseAudio audio, one request per connection."""

import base64
import json

from .base import ConnectionObserver


def _message(raw):
    # The extension ignores frames that are not Kalpa messages; so does this.
    try:
        message = json.loads(raw)
    except (TypeError, ValueError):
        return {}
    return message if isinstance(message, dict) else {}


class KalpaObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        message = _message(raw)
        # The session message and the empty keepalive carry no text to speak.
        if message.get("type") == "sendText" and (message.get("text") or "").strip():
            # Requests are serialized, and an interrupted one ends its connection.
            return request_id
        return None

    def audio_context(self, raw):
        message = _message(raw)
        if message.get("type") != "responseAudio":
            return None
        try:
            audio = base64.b64decode(message.get("pcm_b64") or "", validate=True)
        except (TypeError, ValueError):
            return None
        return self.active_context if audio else None
