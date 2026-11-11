"""Capture DashScope Cosy sentence timestamps at the SDK WebSocket boundary."""

import json
from copy import deepcopy
from fractions import Fraction
from importlib import import_module
from threading import RLock

from .base import ReferenceWord, RequestTimingReference

CLIENT_MODULE = "dashscope.audio.tts_v2.speech_synthesizer"


class CosyTimingHook:
    """Keep an independent provider timing reference for each guarder request."""

    def __init__(self):
        self.lock = RLock()
        self.request_id = None
        self.sample_rate = 0
        self.channels = 1
        self.pcm_bytes = 0
        self.groups = []
        self.emitted = {}
        self.emitted_index_end = None
        self.error = None

    def install(self, monkeypatch, config):
        module = import_module(CLIENT_MODULE)
        hook = self
        original = module.websocket.WebSocketApp

        class ObservedWebSocketApp(original):
            def __init__(self, *args, **kwargs):
                callbacks = dict(kwargs)
                on_message = callbacks.get("on_message")
                on_error = callbacks.get("on_error")
                on_close = callbacks.get("on_close")

                def message(ws, payload):
                    hook.observe_receive(payload)
                    if on_message:
                        on_message(ws, payload)

                def error(ws, error_value):
                    if on_error:
                        on_error(ws, error_value)

                def close(ws, status, reason):
                    if on_close:
                        on_close(ws, status, reason)

                callbacks.update(
                    on_message=message, on_error=error, on_close=close
                )
                super().__init__(*args, **callbacks)

            def send(self, payload, *args, **kwargs):
                hook.observe_send(payload)
                return super().send(payload, *args, **kwargs)

        monkeypatch.setattr(
            module.websocket, "WebSocketApp", ObservedWebSocketApp
        )
        params = config.get("params") or {}
        hook.sample_rate = int(params.get("sample_rate", 0))

    def begin_request(self, request_id):
        with self.lock:
            self.request_id = request_id
            self.pcm_bytes = 0
            self.groups = []
            self.emitted = {}
            self.emitted_index_end = None
            self.error = None

    def observe_send(self, payload):
        return None

    def observe_receive(self, payload):
        with self.lock:
            try:
                if isinstance(payload, (bytes, bytearray)):
                    self.pcm_bytes += len(payload)
                    return
                message = json.loads(payload)
                if message.get("header", {}).get("event") != "result-generated":
                    return
                output = message.get("payload", {}).get("output", {})
                sentence = output.get("sentence") or {}
                index = sentence.get("index", 0)
                raw_words = []
                for item in sentence.get("words") or []:
                    text = item.get("text")
                    begin = item.get("begin_time")
                    end = item.get("end_time")
                    begin_index = item.get("begin_index")
                    end_index = item.get("end_index")
                    if (
                        isinstance(text, str)
                        and text
                        and isinstance(begin, (int, float))
                        and isinstance(end, (int, float))
                    ):
                        raw_words.append(
                            (text, begin, end, begin_index, end_index)
                        )
                indexed = all(
                    begin_index is not None and end_index is not None
                    for _, _, _, begin_index, end_index in raw_words
                ) and bool(raw_words)
                if indexed:
                    boundary = self.emitted_index_end
                    unseen = [
                        word
                        for word in raw_words
                        if boundary is None or word[3] >= boundary
                    ]
                else:
                    emitted = self.emitted.get(index, 0)
                    unseen = raw_words[emitted:]
                limit = (
                    len(unseen)
                    if output.get("type") == "sentence-end"
                    else max(0, len(unseen) - 1)
                )
                if limit <= 0:
                    return
                words = [
                    ReferenceWord(text, Fraction(begin), Fraction(end))
                    for text, begin, end, _, _ in unseen[:limit]
                ]
                if indexed:
                    self.emitted_index_end = max(
                        word[4] for word in unseen[:limit]
                    )
                else:
                    self.emitted[index] = emitted + limit
                if words:
                    self.groups.append(tuple(words))
            except (
                TypeError,
                ValueError,
                KeyError,
                json.JSONDecodeError,
            ) as error:
                self.error = "Cosy capture failed: " + type(error).__name__

    def reference(self, request_id):
        with self.lock:
            error = self.error
            if request_id != self.request_id:
                error = "Cosy capture belongs to another request"
            return RequestTimingReference(
                "Cosy",
                self.sample_rate,
                self.channels,
                self.pcm_bytes,
                tuple(deepcopy(self.groups)),
                error,
            )
