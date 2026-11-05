"""Blaze realtime TTS query frames and binary PCM audio frames."""

import json

from .base import ConnectionObserver


class BlazeObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        # The auth frame ({"token", "strategy"}) is not a text request.
        if not isinstance(raw, str):
            return None
        message = json.loads(raw)
        if str(message.get("query") or "").strip():
            # Blaze serves one query at a time per connection.
            return request_id
        return None

    def audio_context(self, raw):
        # Status events are JSON text; audio is raw binary PCM.
        if isinstance(raw, (bytes, bytearray)) and len(raw) > 0:
            return self.active_context
        return None
