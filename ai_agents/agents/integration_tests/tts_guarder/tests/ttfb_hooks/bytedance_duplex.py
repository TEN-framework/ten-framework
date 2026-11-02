"""ByteDance duplex session IDs and audio-bearing event payloads."""

import gzip
import json

from .base import ConnectionObserver


def session_payload(raw, event):
    if not isinstance(raw, bytes) or len(raw) < 16:
        return None
    offset = (raw[0] & 15) * 4
    if not raw[1] & 4 or int.from_bytes(raw[offset : offset + 4], "big") != event:
        return None
    offset += 4
    size = int.from_bytes(raw[offset : offset + 4], "big")
    offset += 4
    session = raw[offset : offset + size].decode()
    offset += size
    size = int.from_bytes(raw[offset : offset + 4], "big")
    payload = raw[offset + 4 : offset + 4 + size]
    if raw[2] & 15 == 1:
        payload = gzip.decompress(payload)
    return session, payload


class ByteDanceDuplexObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        parsed = session_payload(raw, 200)
        if parsed is not None:
            session, payload = parsed
            if json.loads(payload).get("req_params", {}).get("text", "").strip():
                return session
        return None

    def audio_context(self, raw):
        parsed = session_payload(raw, 352)
        if parsed is not None and parsed[1]:
            return parsed[0]
        return None
