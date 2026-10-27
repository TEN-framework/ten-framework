"""Deferred, independent provider observations keyed by tester request ID."""

from threading import RLock
import time


class Reference:
    def __init__(self):
        self.lock = RLock()
        self.request_id = None
        self.events = []
        self.sent = {}
        self.audio = {}

    def begin_request(self, request_id):
        with self.lock:
            self.request_id = request_id

    def current_request(self):
        with self.lock:
            return self.request_id

    def capture(self, kind, payload=True, predicate=bool, request_id=None):
        timestamp = time.monotonic_ns()
        with self.lock:
            self.events.append(
                (timestamp, request_id or self.request_id, kind, payload, predicate)
            )

    def elapsed_ms(self, request_id):
        with self.lock:
            events, self.events = self.events, []
        # Decode outside the capture lock, so provider callbacks stay cheap.
        for timestamp, captured_id, kind, payload, predicate in sorted(
            events, key=lambda event: event[0]
        ):
            if captured_id is None or not predicate(payload):
                continue
            with self.lock:
                if kind == "text":
                    self.sent.setdefault(captured_id, timestamp)
                elif captured_id in self.sent:
                    self.audio.setdefault(captured_id, timestamp)
        with self.lock:
            if request_id not in self.sent or request_id not in self.audio:
                return None
            return (self.audio[request_id] - self.sent[request_id]) / 1_000_000
