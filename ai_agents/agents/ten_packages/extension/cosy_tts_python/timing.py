"""First-text/first-audio timing for one active provider request."""

from threading import Lock
import time


class TTSRequestTiming:
    def __init__(self) -> None:
        self._lock = Lock()
        self._request_id: str | None = None
        self._sent_ns: int | None = None
        self._received_ns: int | None = None

    def text_sent(self, request_id: str) -> None:
        timestamp = time.monotonic_ns()
        with self._lock:
            if request_id != self._request_id:
                self._request_id = request_id
                self._sent_ns = timestamp
                self._received_ns = None

    def audio_received(self, request_id: str, received_ns: int | None = None) -> None:
        timestamp = time.monotonic_ns() if received_ns is None else received_ns
        with self._lock:
            if request_id == self._request_id and self._received_ns is None:
                self._received_ns = timestamp

    def elapsed_ms(self, request_id: str) -> int | None:
        with self._lock:
            if (
                request_id != self._request_id
                or self._sent_ns is None
                or self._received_ns is None
            ):
                return None
            return (self._received_ns - self._sent_ns) // 1_000_000
