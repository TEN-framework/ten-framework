"""Soul JSON and binary gateway frames, excluding control and empty data."""

import base64
import gzip
import json
import struct

from .base import ConnectionObserver


def decode_message(raw):
    """Decode the gateway envelope independently of the extension decoder."""
    if isinstance(raw, str):
        return json.loads(raw)
    magic, version, _command, serialization, compression, length, count = struct.unpack(
        "!IBBBBIB", raw[:13]
    )
    if magic != 0x736F756C or version != 1 or serialization != 1:
        raise ValueError("Unsupported Soul gateway envelope")
    if length != len(raw) - 13:
        raise ValueError("Invalid Soul gateway length")
    offset = 13
    for _ in range(count):
        _, size = struct.unpack("!BI", raw[offset : offset + 5])
        offset += 5 + size
        if offset > len(raw):
            raise ValueError("Invalid Soul gateway header")
    body = raw[offset:]
    if compression == 1:
        body = gzip.decompress(body)
    elif compression != 0:
        raise ValueError("Unsupported Soul gateway compression")
    return json.loads(body)


class SoulObserver(ConnectionObserver):
    def text_context(self, raw, request_id):
        message = decode_message(raw)
        if message.get("event") == "task_continue" and message.get("text", "").strip():
            return request_id
        return None

    def audio_context(self, raw):
        message = decode_message(raw)
        audio = (message.get("data") or {}).get("audio")
        if message.get("event") == "task_continued" and audio:
            if base64.b64decode(audio, validate=True):
                return self.active_context
        return None
