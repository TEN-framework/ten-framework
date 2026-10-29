"""Associate wire contexts with tester requests, independently per connection."""

import time


class ConnectionObserver:
    def __init__(self, reference):
        self.reference = reference
        self.requests = {}
        self.active_context = None

    def text_sent(self, context, timestamp, request_id):
        if request_id is not None:
            self.requests[context] = request_id
            self.active_context = context
            self.reference.text_sent(request_id, timestamp)

    def audio_received(self, context, timestamp):
        request_id = self.requests.get(context)
        if request_id is not None:
            self.reference.audio_received(request_id, timestamp)

    def observe_send(self, raw):
        self.reference.defer_event(self._consume_send, raw, time.monotonic_ns())

    def observe_receive(self, raw):
        self.reference.defer_event(self._consume_audio, raw, time.monotonic_ns())

    def _consume_send(self, raw, timestamp, request_id):
        context = self.text_context(raw, request_id)
        if context is not None:
            self.text_sent(context, timestamp, request_id)

    def _consume_audio(self, raw, timestamp, request_id):
        context = self.audio_context(raw)
        if context is not None:
            self.audio_received(context, timestamp)
