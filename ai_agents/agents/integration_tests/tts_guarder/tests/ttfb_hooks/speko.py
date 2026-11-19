"""Speko Router input.append text and nonempty binary PCM audio frames."""

import json

from .base import ConnectionObserver


class SpekoObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        # configure/commit/cancel/close are control frames, never text.
        if not isinstance(raw, str):
            return None
        message = json.loads(raw)
        text = message.get("text")
        if message.get("type") == "input.append" and isinstance(text, str) and text.strip():
            # One utterance is in flight per Router session.
            return request_id
        return None

    def audio_context(self, raw):
        # Router events are JSON text frames; only binary frames carry PCM.
        if isinstance(raw, bytes) and raw:
            return self.active_context
        return None
