#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch

from blaze_tts_python.blaze_tts import EVENT_TTS_ERROR

from blaze_tts_python.tests.mock import (
    BASIC_CONFIG,
    RecordingTester,
    install_mock_client,
)


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_sequential_requests_each_get_one_lifecycle(MockBlazeTTSClient):
    install_mock_client(MockBlazeTTSClient, chunks=2)

    tester = RecordingTester(
        [
            ("req-1", "Câu một.", True),
            ("req-2", "Câu hai.", True),
        ],
        expected_ends=2,
    )
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    order = [
        (name, payload.get("request_id"))
        for name, payload in tester.events
        if name in ("tts_audio_start", "tts_audio_end")
    ]
    assert order == [
        ("tts_audio_start", "req-1"),
        ("tts_audio_end", "req-1"),
        ("tts_audio_start", "req-2"),
        ("tts_audio_end", "req-2"),
    ]


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_appended_input_shares_one_lifecycle(MockBlazeTTSClient):
    install_mock_client(MockBlazeTTSClient)

    tester = RecordingTester(
        [
            ("req-append", "Một. ", False),
            ("req-append", "Hai. ", False),
            ("req-append", "Ba.", True),
        ]
    )
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    assert tester.count("tts_audio_start") == 1
    assert tester.count("tts_audio_end") == 1
    assert tester.payloads("tts_audio_end")[0]["reason"] == 1  # REQUEST_END


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_failed_request_releases_next_request(MockBlazeTTSClient):
    instance = install_mock_client(MockBlazeTTSClient)
    real_get = instance.get.side_effect

    async def failing_first(text: str):
        if text == "hỏng":
            yield b"temporarily unavailable", EVENT_TTS_ERROR
            return
        async for item in real_get(text):
            yield item

    instance.get.side_effect = failing_first

    tester = RecordingTester(
        [
            ("req-fail", "hỏng", True),
            ("req-ok", "ổn", True),
        ],
        expected_ends=2,
    )
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    ends = {
        p["request_id"]: p["reason"] for p in tester.payloads("tts_audio_end")
    }
    assert ends == {"req-fail": 3, "req-ok": 1}  # ERROR, REQUEST_END
