#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""Capture only MiniMax audio settings, raw subtitles, and sentence offsets."""

import json
from copy import deepcopy
from importlib import import_module
from threading import RLock

from .base import CapturedSubtitle, CapturedTiming
from .transport import ObservedWebSockets

DEFAULT_URL = "wss://api.minimax.cn/ws/v1/t2a_v2_bidi"
CLIENT_MODULE = (
    "ten_packages.extension.minimax_tts_websocket_duplex.minimax_tts"
)


class MiniMaxTimingHook:
    """Capture facts for the test's current sequential TEN request."""

    def __init__(self):
        self.audio_setting = {}
        self.request_id = None
        self.pcm_bytes = 0
        self.sentence_start_bytes = 0
        self.subtitles = []
        self.error = None
        self.lock = RLock()

    def install(self, monkeypatch, config):
        module = import_module(CLIENT_MODULE)
        dependency = ObservedWebSockets(
            module.websockets, config.get("url") or DEFAULT_URL, self
        )
        monkeypatch.setattr(module, "websockets", dependency)

    def begin_request(self, request_id):
        with self.lock:
            self.request_id = request_id
            self.pcm_bytes = 0
            self.sentence_start_bytes = 0
            self.subtitles = []

    def observe_send(self, payload):
        self._observe(payload, outgoing=True)

    def observe_receive(self, payload):
        self._observe(payload, outgoing=False)

    def _observe(self, payload, outgoing):
        with self.lock:
            try:
                message = json.loads(payload)
                if outgoing:
                    if message.get("event") == "task_start":
                        self.audio_setting = deepcopy(
                            message.get("audio_setting", {})
                        )
                elif self.request_id is not None:
                    self._capture_response(message)
            # Capture failures must not change the production wire path.
            except Exception as error:  # noqa: BLE001
                self.error = "MiniMax capture failed: " + type(error).__name__

    def _capture_response(self, message):
        event = message.get("event")
        if event == "sentence_start":
            self.sentence_start_bytes = self.pcm_bytes
        elif event == "task_continued":
            data = message.get("data") or {}
            self.pcm_bytes += len(data.get("audio") or "") // 2
            subtitle = data.get("subtitle")
            if subtitle and subtitle.get("timestamped_words"):
                self.subtitles.append(
                    CapturedSubtitle(
                        self.sentence_start_bytes, deepcopy(subtitle)
                    )
                )

    def capture(self, request_id):
        with self.lock:
            error = self.error
            if request_id != self.request_id:
                error = "MiniMax capture belongs to another request"
            return CapturedTiming(
                deepcopy(self.audio_setting),
                self.pcm_bytes,
                tuple(deepcopy(self.subtitles)),
                error,
            )
