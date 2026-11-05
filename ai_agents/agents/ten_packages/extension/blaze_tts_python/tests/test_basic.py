#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch

from blaze_tts_python.tests.mock import (
    AUDIO_CHUNK,
    BASIC_CONFIG,
    RecordingTester,
    install_mock_client,
)


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_each_fragment_is_synthesized(MockBlazeTTSClient):
    texts_seen: list[str] = []
    install_mock_client(MockBlazeTTSClient, texts_seen)

    tester = RecordingTester(
        [
            ("tts_request_1", "Xin chào. ", False),
            ("tts_request_1", "Tôi là Blaze. ", False),
            ("tts_request_1", "", True),
        ]
    )
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    assert [t for t in texts_seen if t.strip()] == [
        "Xin chào. ",
        "Tôi là Blaze. ",
    ]
    assert tester.count("tts_audio_start") == 1
    assert tester.count("tts_audio_end") == 1
    assert tester.audio_bytes == 2 * len(AUDIO_CHUNK)


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_event_order_and_duration(MockBlazeTTSClient):
    install_mock_client(MockBlazeTTSClient, chunks=5)

    tester = RecordingTester([("req-1", "Xin chào.", True)])
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    names = [name for name, _ in tester.events if name.startswith("tts_")]
    assert names.index("tts_audio_start") < names.index("tts_audio_end")
    end = tester.payloads("tts_audio_end")[0]
    assert end["request_id"] == "req-1"
    # 5 chunks x 20 ms at 24 kHz
    assert end["request_total_audio_duration_ms"] == 100


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_empty_text_ends_without_audio(MockBlazeTTSClient):
    install_mock_client(MockBlazeTTSClient)

    tester = RecordingTester([("req-empty", "   ", True)])
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    assert tester.count("tts_audio_end") == 1
    assert tester.audio_bytes == 0
