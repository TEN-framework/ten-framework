#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
from typing import Any

from ..websocket import SonioxTranscriptToken
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override


ExpectedResults = list[tuple[bool, str]]


class SonioxAsrMixedResultTester(AsyncExtensionTester):

    def __init__(self, expected_batches: list[ExpectedResults]):
        super().__init__()
        self.expected_batches = expected_batches
        self.received_batches: list[ExpectedResults] = []
        self.received_results: ExpectedResults = []

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> bool:
        if success:
            return False

        err = TenError.create(
            error_code=TenErrorCode.ErrorCodeGeneric,
            error_message=error_message,
        )
        ten_env_tester.stop_test(err)
        return True

    def assert_next_batch(
        self,
        ten_env_tester: AsyncTenEnvTester,
        data_dict: dict[str, Any],
    ) -> None:
        results = data_dict.get("results", [])
        actual = [(item.get("final"), item.get("text")) for item in results]
        batch_index = len(self.received_batches)

        if self.stop_test_if_checking_failed(
            ten_env_tester,
            batch_index < len(self.expected_batches),
            f"unexpected asr_results batch: {actual}",
        ):
            return

        expected = self.expected_batches[batch_index]
        if self.stop_test_if_checking_failed(
            ten_env_tester,
            actual == expected,
            f"asr_results mismatch: expected {expected}, got {actual}",
        ):
            return

        self.received_batches.append(actual)
        if len(self.received_batches) == len(self.expected_batches):
            ten_env_tester.stop_test()

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        ten_env_tester.log_info(f"tester on_data, data_name: {data_name}")

        if data_name == "asr_result":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.received_results.append(
                (data_dict.get("final"), data_dict.get("text"))
            )
            return

        if data_name == "asr_results":
            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)
            self.assert_next_batch(ten_env_tester, data_dict)
            return

        if data_name == "error":
            data_json, _ = data.get_property_to_json()
            self.stop_test_if_checking_failed(
                ten_env_tester,
                False,
                f"unexpected error data: {data_json}",
            )


def _token(
    text: str,
    *,
    is_final: bool,
    start_ms: int,
    end_ms: int,
    language: str = "zh",
) -> SonioxTranscriptToken:
    return SonioxTranscriptToken(
        text=text,
        start_ms=start_ms,
        end_ms=end_ms,
        is_final=is_final,
        language=language,
    )


def _property_json() -> dict[str, Any]:
    return {
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }


def test_mixed_final_and_non_final_tokens_publish_asr_results_batch(
    patch_soniox_ws,
):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [
                _token("喂", is_final=True, start_ms=1020, end_ms=1260),
                _token("喂", is_final=True, start_ms=1380, end_ms=1620),
                _token("你", is_final=False, start_ms=1800, end_ms=1920),
            ],
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxAsrMixedResultTester(
        expected_batches=[
            [
                (True, "喂喂"),
                (False, "你"),
            ],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_mixed_final_and_non_final err: {err}"


def test_mixed_language_final_tokens_stay_split_inside_batch(
    patch_soniox_ws,
):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [
                _token(
                    "你好",
                    is_final=True,
                    start_ms=1000,
                    end_ms=1400,
                    language="zh",
                ),
                _token(
                    "hello",
                    is_final=True,
                    start_ms=1400,
                    end_ms=1800,
                    language="en",
                ),
                _token(
                    "world",
                    is_final=False,
                    start_ms=1800,
                    end_ms=2200,
                    language="en",
                ),
            ],
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxAsrMixedResultTester(
        expected_batches=[
            [
                (True, "你好"),
                (True, "hello"),
                (False, "world"),
            ],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_mixed_language_final_tokens err: {err}"


def test_homogeneous_non_final_tokens_publish_single_item_batch(
    patch_soniox_ws,
):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [
                _token(
                    "hello ",
                    is_final=False,
                    start_ms=1000,
                    end_ms=1300,
                    language="en",
                ),
                _token(
                    "world",
                    is_final=False,
                    start_ms=1300,
                    end_ms=1700,
                    language="en",
                ),
            ],
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxAsrMixedResultTester(
        expected_batches=[
            [
                (False, "hello world"),
            ],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_homogeneous_non_final_tokens err: {err}"
