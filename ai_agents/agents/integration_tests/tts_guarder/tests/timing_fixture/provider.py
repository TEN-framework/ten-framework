#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
"""A loopback WebSocket provider with fixed PCM and word annotations."""

import json
import threading
from contextlib import suppress

from websockets.exceptions import ConnectionClosed
from websockets.sync.server import serve


class TimingProvider:
    def __init__(self, scenario):
        self.scenario = scenario
        self.server = serve(self.handle, "127.0.0.1", 0)
        self.url = f"ws://127.0.0.1:{self.server.socket.getsockname()[1]}"
        self.thread = threading.Thread(target=self.server.serve_forever)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, _kind, _value, _traceback):
        self.server.shutdown()
        self.thread.join(timeout=2)
        assert not self.thread.is_alive()

    def handle(self, connection):
        request_index = 0
        with suppress(ConnectionClosed):
            for payload in connection:
                event = json.loads(payload)["event"]
                match event:
                    case "task_start":
                        connection.send(json.dumps({"event": "task_started"}))
                    case "task_continue":
                        self.send_sentence(connection, request_index)
                        request_index += 1
                    case "task_flush":
                        connection.send(json.dumps({"event": "task_flushed"}))

    def send_sentence(self, connection, request_index):
        fractional = self.scenario in {
            "cumulative_drift",
            "fractional_clock",
        } or (self.scenario == "separate_origins" and request_index == 0)
        samples = 17113 if fractional else 1600
        count = 8 if fractional else 1
        word_start, word_end, result_end = 42.6666666667, 60, 60
        if self.scenario == "word_out_of_bounds":
            word_start, word_end, result_end = 90, 110, 100
        if self.scenario == "result_out_of_bounds":
            result_end = 142
        subtitle = {
            "text": "word",
            "time_begin": 0,
            "time_end": result_end,
            "timestamped_words": [
                {
                    "word": "word",
                    "word_begin": 0,
                    "word_end": 4,
                    "time_begin": word_start,
                    "time_end": word_end,
                }
            ],
        }
        connection.send(json.dumps({"event": "sentence_start"}))
        for index in range(count):
            data = {"audio": "0001" * samples}
            if index == 0:
                data["subtitle"] = subtitle
            connection.send(
                json.dumps({"event": "task_continued", "data": data})
            )
