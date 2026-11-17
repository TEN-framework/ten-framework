#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from unittest.mock import patch

from blaze_tts_python.blaze_tts import (
    EVENT_TTS_ERROR,
    BlazeTTSConnectionException,
)
from blaze_tts_python.tests.mock import (
    BASIC_CONFIG,
    RecordingTester,
    install_mock_client,
)


def test_missing_api_key_fatal_error():
    """A missing key is reported once as a FATAL error at init."""
    tester = RecordingTester([], stop_on_error=True)
    tester.set_test_mode_single(
        "blaze_tts_python", json.dumps({"params": {"api_key": ""}})
    )
    tester.run()

    error = tester.payloads("error")[0]
    assert error["code"] == -1000
    assert "API key" in error["message"]


def test_invalid_url_fatal_error():
    config = {"params": {"api_key": "k", "base_url": "not-a-url"}}
    tester = RecordingTester([], stop_on_error=True)
    tester.set_test_mode_single("blaze_tts_python", json.dumps(config))
    tester.run()

    error = tester.payloads("error")[0]
    assert error["code"] == -1000
    assert "base_url" in error["message"]


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_invalid_api_key_at_startup_is_fatal(MockBlazeTTSClient):
    instance = install_mock_client(MockBlazeTTSClient)
    instance.start.side_effect = BlazeTTSConnectionException(
        status_code=401, body="failed-authentication"
    )

    tester = RecordingTester([], stop_on_error=True)
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    error = tester.payloads("error")[0]
    assert error["code"] == -1000
    assert error["vendor_info"]["vendor"] == "blaze"
    assert error["vendor_info"]["code"] == "401"


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_auth_rejected_during_request_is_fatal(MockBlazeTTSClient):
    instance = install_mock_client(MockBlazeTTSClient)

    async def rejected(_text):
        raise BlazeTTSConnectionException(
            status_code=401, body="failed-authentication"
        )
        yield  # pragma: no cover

    instance.get.side_effect = rejected

    tester = RecordingTester([("req-key", "hello", True)])
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    error = tester.payloads("error")[0]
    assert error["code"] == -1000
    assert error["vendor_info"]["code"] == "401"
    assert tester.count("tts_audio_end") == 1


@patch("blaze_tts_python.extension.BlazeTTSClient")
def test_failed_request_is_non_fatal_and_ends_request(MockBlazeTTSClient):
    install_mock_client(
        MockBlazeTTSClient,
        events=[(b"temporarily unavailable", EVENT_TTS_ERROR)],
    )

    tester = RecordingTester([("req-fail", "hello", True)])
    tester.set_test_mode_single("blaze_tts_python", json.dumps(BASIC_CONFIG))
    tester.run()

    error = tester.payloads("error")[0]
    assert error["code"] == 1000
    assert tester.count("tts_audio_end") == 1
    assert tester.payloads("tts_audio_end")[0]["reason"] == 3  # ERROR
