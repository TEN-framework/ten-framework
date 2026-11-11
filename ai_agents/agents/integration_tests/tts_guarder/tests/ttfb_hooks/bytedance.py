"""ByteDance standard binary request/audio frames."""

import gzip
import json

from .base import ConnectionObserver


class ByteDanceObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        if not isinstance(raw, bytes) or len(raw) < 8 or raw[1] >> 4 != 1:
            return None
        offset = (raw[0] & 15) * 4
        size = int.from_bytes(raw[offset : offset + 4], "big")
        payload = raw[offset + 4 : offset + 4 + size]
        if raw[2] & 15 == 1:
            payload = gzip.decompress(payload)
        request = json.loads(payload).get("request", {})
        if request.get("text", "").strip():
            return request.get("reqid") or request_id
        return None

    def audio_context(self, raw):
        if not isinstance(raw, bytes) or len(raw) < 8 or raw[1] >> 4 != 11:
            return None
        offset = (raw[0] & 15) * 4
        if raw[1] & 15:
            offset += 4
        size = int.from_bytes(raw[offset : offset + 4], "big")
        if size and raw[offset + 4 : offset + 4 + size]:
            return self.active_context
        return None
