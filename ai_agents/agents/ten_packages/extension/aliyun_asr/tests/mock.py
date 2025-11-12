#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
import sys
from types import SimpleNamespace
import types
import pytest
from unittest.mock import AsyncMock, MagicMock, patch


@pytest.fixture(scope="function")
def patch_aliyun_ws():
    # Step 1: create mock `nls.token` module
    fake_token_module = types.ModuleType("nls.token")
    fake_token_module.getToken = MagicMock(return_value="mocked_token")

    # Step 2: create mock `nls` module
    fake_nls = types.ModuleType("nls")

    # Attach `token` as attribute (so nls.token.getToken(...) works)
    fake_nls.token = fake_token_module

    # Step 3: callbacks store
    callback_store = {}

    class MockNlsSpeechTranscriber:
        def __init__(self, **kwargs):
            callback_store.update(
                {
                    "on_start": kwargs.get("on_start"),
                    "on_sentence_end": kwargs.get("on_sentence_end"),
                    "on_error": kwargs.get("on_error"),
                    "on_close": kwargs.get("on_close"),
                }
            )

        def start(self, *args, **kwargs):
            print("[mock] start() called")
            if callback_store.get("on_start"):
                callback_store["on_start"]("mock_start_event")

            async def delayed_sentence():
                await asyncio.sleep(1)
                if callback_store.get("on_sentence_end"):
                    result_json = json.dumps(
                        {
                            "payload": {"result": "hello world"},
                            "header": {"name": "SentenceEnd"},
                        }
                    )
                    callback_store["on_sentence_end"](result_json)

            asyncio.get_event_loop().create_task(delayed_sentence())

        def send_audio(self, data):
            print("[mock] send_audio")

        def stop(self):
            print("[mock] stop()")

    fake_nls.NlsSpeechTranscriber = MockNlsSpeechTranscriber

    # Step 4: inject both nls and nls.token into sys.modules
    with patch.dict(
        sys.modules,
        {
            "nls": fake_nls,
            "nls.token": fake_token_module,
        },
    ):
        yield fake_nls
