#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""MiniMax Duplex wire capture, independent of extension timing state."""

import json
from dataclasses import dataclass, field
from fractions import Fraction
from importlib import import_module
from threading import RLock

from .base import ReferenceWord, RequestTimingReference
from .transport import ObservedWebSockets

DEFAULT_URL = "wss://api.minimax.cn/ws/v1/t2a_v2_bidi"
CLIENT_MODULE = (
    "ten_packages.extension.minimax_tts_websocket_duplex.minimax_tts"
)


@dataclass
class ConnectionCapture:
    sample_rate: int = 0
    channels: int = 1
    request_id: str | None = None


@dataclass
class RequestCapture:
    connection_id: int | None = None
    sample_rate: int = 0
    channels: int = 1
    pcm_bytes: int = 0
    sentence_start_bytes: int = 0
    captions: list[tuple[ReferenceWord, ...]] = field(default_factory=list)
    complete: bool = False


class MiniMaxTimingHook:
    """Keep request and connection capture private to one test invocation."""

    def __init__(self, request_ids):
        self.requests = {key: RequestCapture() for key in request_ids}
        self.connections: dict[int, ConnectionCapture] = {}
        self.expected_request: str | None = None
        self.error: str | None = None
        self.lock = RLock()

    def install(self, monkeypatch, config):
        module = import_module(CLIENT_MODULE)
        dependency = ObservedWebSockets(
            module.websockets, config.get("url") or DEFAULT_URL, self
        )
        monkeypatch.setattr(module, "websockets", dependency)

    def begin_request(self, request_id):
        with self.lock:
            if (
                self.expected_request
                and not self.requests[self.expected_request].complete
            ):
                self.error = "MiniMax timing hook received overlapping requests"
            self.expected_request = request_id

    def open_connection(self):
        with self.lock:
            connection_id = len(self.connections) + 1
            self.connections[connection_id] = ConnectionCapture()
            return connection_id

    def observe_send(self, connection_id, payload):
        self._observe(connection_id, payload, outgoing=True)

    def observe_receive(self, connection_id, payload):
        self._observe(connection_id, payload, outgoing=False)

    def _observe(self, connection_id, payload, outgoing):
        with self.lock:
            try:
                message = json.loads(payload)
                if outgoing:
                    self._sent(connection_id, message)
                else:
                    self._received(connection_id, message)
            # Parser failures must not change the production wire path.
            except Exception as error:  # noqa: BLE001
                if self.error is None:
                    self.error = (
                        "MiniMax timing hook capture failed: "
                        + type(error).__name__
                    )

    def _sent(self, connection_id, message):
        connection = self.connections[connection_id]
        event = message.get("event")
        if event == "task_start":
            setting = message.get("audio_setting", {})
            connection.sample_rate = setting.get("sample_rate", 0)
            connection.channels = setting.get(
                "channels", setting.get("channel", 1)
            )
            if setting.get("format", "pcm") != "pcm":
                self.error = "MiniMax timing hook requires PCM audio"
        elif event == "task_continue":
            self._bind_request(connection_id)

    def _bind_request(self, connection_id):
        request_id = self.expected_request
        if request_id is None:
            self.error = "MiniMax input has no matching TEN request"
            return
        capture = self.requests[request_id]
        connection = self.connections[connection_id]
        if capture.complete or capture.connection_id not in (
            None,
            connection_id,
        ):
            self.error = "MiniMax request has ambiguous connection ownership"
            return
        connection.request_id = request_id
        capture.connection_id = connection_id
        capture.sample_rate = connection.sample_rate
        capture.channels = connection.channels

    def _received(self, connection_id, message):
        connection = self.connections[connection_id]
        event = message.get("event")
        if event not in {
            "sentence_start",
            "task_continued",
            "task_flushed",
        }:
            return
        if connection.request_id is None:
            self.error = "MiniMax response has no matching request"
            return
        capture = self.requests[connection.request_id]
        if event == "sentence_start":
            capture.sentence_start_bytes = capture.pcm_bytes
        elif event == "task_continued":
            self._capture_output(capture, message.get("data") or {})
        else:
            capture.complete = True
            connection.request_id = None

    def _capture_output(self, capture, data):
        audio_size = len(bytes.fromhex(data.get("audio") or ""))
        capture.pcm_bytes += audio_size
        words = (data.get("subtitle") or {}).get("timestamped_words", [])
        if not words:
            return
        if (
            sum(len(caption) for caption in capture.captions) + len(words)
            > 10000
        ):
            self.error = "MiniMax timing reference exceeds capture limit"
            return
        sentence_ms = Fraction(
            capture.sentence_start_bytes * 1000,
            2 * capture.channels * capture.sample_rate,
        )
        merged = []
        for item in words:
            if (
                merged
                and item.get("word_begin") is not None
                and item.get("word_end") is not None
                and (item["word_begin"], item["word_end"])
                == (merged[-1].get("word_begin"), merged[-1].get("word_end"))
            ):
                merged[-1]["time_end"] = item["time_end"]
            else:
                merged.append(dict(item))
        capture.captions.append(
            tuple(
                ReferenceWord(
                    " " if item["word"] == "[SPACE]" else item["word"],
                    sentence_ms + Fraction(str(item["time_begin"])),
                    sentence_ms + Fraction(str(item["time_end"])),
                )
                for item in merged
            )
        )

    def reference(self, request_id):
        with self.lock:
            capture = self.requests[request_id]
            return RequestTimingReference(
                "MiniMax",
                capture.sample_rate,
                capture.channels,
                capture.pcm_bytes,
                tuple(capture.captions),
                capture.complete,
                self.error,
            )
