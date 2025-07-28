#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
import pytest
from unittest.mock import MagicMock, patch


@pytest.fixture(scope="function")
def patch_azure_ws():
    patch_target = "ten_packages.extension.azure_asr_python.extension.speechsdk.SpeechRecognizer"

    with patch(patch_target) as MockRecognizer, patch(
        "ten_packages.extension.azure_asr_python.extension.speechsdk.SpeechConfig"
    ) as MockSpeechConfig, patch(
        "ten_packages.extension.azure_asr_python.extension.speechsdk.audio.AudioConfig"
    ) as MockAudioConfig, patch(
        "ten_packages.extension.azure_asr_python.extension.speechsdk.audio.PushAudioInputStream"
    ) as MockStream, patch(
        "ten_packages.extension.azure_asr_python.extension.speechsdk.audio.AudioStreamFormat"
    ) as MockStreamFormat:

        recognizer_instance = MagicMock()
        event_handlers = {}
        patch_azure_ws.event_handlers = event_handlers

        def connect_mock(handler):
            event_handlers["recognized"] = handler

        recognizer_instance.recognized.connect.side_effect = connect_mock

        MockRecognizer.return_value = recognizer_instance
        MockSpeechConfig.return_value = MagicMock()
        MockAudioConfig.return_value = MagicMock()
        MockStream.return_value = MagicMock()
        MockStreamFormat.return_value = MagicMock()

        fixture_obj = SimpleNamespace(
            recognizer_instance=recognizer_instance,
            event_handlers=event_handlers,
        )

        yield fixture_obj
