"""MiniMax task_continue text and task_continued audio, in both WS modes."""

import json

from .base import ConnectionObserver


class MiniMaxObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        message = json.loads(raw)
        if message.get("event") == "task_continue" and message.get("text", "").strip():
            # This protocol has one outstanding text request per connection.
            return request_id
        return None

    def audio_context(self, raw):
        message = json.loads(raw)
        audio = (message.get("data") or {}).get("audio")
        if message.get("event") == "task_continued" and audio and bytes.fromhex(audio):
            return self.active_context
        return None
