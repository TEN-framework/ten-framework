#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import asyncio
import json
from typing import Any

import pytest

from ..text_utils import SentenceBoundaryDetector
from ..websocket import SonioxFinToken, SonioxTranscriptToken
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override


ExpectedResults = list[tuple[bool, str]]


class SonioxSentenceEndTester(AsyncExtensionTester):

    def __init__(self, expected_batches: list[ExpectedResults]):
        super().__init__()
        self.expected_batches = expected_batches
        self.received_batches: list[ExpectedResults] = []

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


def _chars(text: str) -> list[str]:
    return list(text)


def _tokens(
    text: str,
    *,
    is_final: bool,
    start_ms: int = 1000,
    language: str = "zh",
) -> list[SonioxTranscriptToken]:
    return [
        SonioxTranscriptToken(
            text=ch,
            start_ms=start_ms + index * 10,
            end_ms=start_ms + index * 10 + 8,
            is_final=is_final,
            language=language,
        )
        for index, ch in enumerate(text)
    ]


def _property_json() -> dict[str, Any]:
    return {
        "holding_mode": "sentence_terminator",
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        },
    }


@pytest.mark.parametrize(
    ("text", "language", "expected"),
    [
        pytest.param("台北选手戴资颖", "zh-CN", False, id="zh_no_terminal"),
        pytest.param("Hello world", "en-US", False, id="en_no_terminal"),
        pytest.param("189，当日", "zh-CN", False, id="zh_comma"),
        pytest.param("note;", "en-US", False, id="en_semicolon"),
        pytest.param("比赛结束。", "zh-CN", True, id="zh_full_stop"),
        pytest.param("Hello world.", "en-US", True, id="en_period"),
        pytest.param("What?", "en-US", True, id="en_question"),
        pytest.param("你好!", "zh-CN", True, id="mixed_bang"),
        pytest.param("Mr.", "en-US", False, id="en_abbreviation"),
        pytest.param("U.S. team", "en-US", False, id="en_us_abbreviation"),
        pytest.param("2.3", "en-US", False, id="en_decimal"),
        pytest.param("really!!!", "en-US", True, id="en_bang_cluster"),
        pytest.param("等等……", "zh-CN", True, id="zh_ellipsis"),
    ],
)
def test_ends_with_sentence_end_punctuation(
    text: str,
    language: str,
    expected: bool,
) -> None:
    detector = SentenceBoundaryDetector()
    assert detector.ends_with_boundary(text, language) is expected


@pytest.mark.parametrize(
    ("text", "language"),
    [
        pytest.param("Bonjour le monde.", "fr-FR", id="fr"),
        pytest.param("こんにちは。", "ja-JP", id="ja"),
        pytest.param("안녕하세요.", "ko-KR", id="ko"),
    ],
)
def test_non_zh_en_languages_skip_sentence_boundary_detection(
    text: str,
    language: str,
) -> None:
    detector = SentenceBoundaryDetector()
    assert not detector.supports_language(language)
    assert not detector.ends_with_boundary(text, language)
    assert detector.split_at_last_complete_sentence(_chars(text), language) == (
        0,
        0,
    )


@pytest.mark.parametrize(
    ("token_texts", "language", "expected"),
    [
        pytest.param(_chars("你好。"), "zh-CN", (3, 3), id="zh_complete"),
        pytest.param(
            _chars("Hello world."),
            "en-US",
            (12, 12),
            id="en_complete",
        ),
        pytest.param(
            _chars("你好。世界"),
            "zh-CN",
            (3, 3),
            id="zh_prefix_suffix",
        ),
        pytest.param(
            _chars("Hello. More"),
            "en-US",
            (6, 6),
            id="en_prefix_suffix",
        ),
        pytest.param(
            _chars("台北选手戴资颖"),
            "zh-CN",
            (0, 0),
            id="zh_no_boundary",
        ),
        pytest.param(
            _chars("12.5"),
            "zh-CN",
            (0, 0),
            id="decimal_not_boundary",
        ),
        pytest.param(
            _chars("really!!!hello"),
            "en-US",
            (9, 9),
            id="terminator_cluster",
        ),
    ],
)
def test_split_at_last_complete_sentence(
    token_texts: list[str],
    language: str,
    expected: tuple[int, int],
) -> None:
    detector = SentenceBoundaryDetector()
    assert (
        detector.split_at_last_complete_sentence(token_texts, language)
        == expected
    )


def test_vendor_final_without_sentence_end_is_sent_as_non_final(
    patch_soniox_ws,
):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("台北选手戴资颖", is_final=True),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(False, "台北选手戴资颖")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_vendor_final_without_sentence_end err: {err}"


def test_trailing_punctuation_completes_deferred_sentence(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("台北选手戴资颖", is_final=True),
            0,
            0,
        )
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("。", is_final=True, start_ms=2000),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(False, "台北选手戴资颖")],
            [(True, "台北选手戴资颖。")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_trailing_punctuation_completes err: {err}"


def test_sentence_prefix_emits_and_suffix_is_deferred(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("比赛结束。下一场", is_final=True),
            0,
            0,
        )
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("开始了。", is_final=True, start_ms=2000),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(True, "比赛结束。")],
            [(True, "下一场开始了。")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_sentence_prefix_emits err: {err}"


def test_mixed_vendor_final_and_non_final_publish_two_results(
    patch_soniox_ws,
):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens(
                "Hello world.",
                is_final=True,
                language="en",
            )
            + _tokens(
                " More text",
                is_final=False,
                start_ms=2000,
                language="en",
            ),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [
                (True, "Hello world."),
                (False, " More text"),
            ],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_mixed_vendor_final_and_non_final err: {err}"


def test_english_abbreviation_waits_for_sentence_end(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("Mr. Smith", is_final=True, language="en"),
            0,
            0,
        )
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens(
                " went home.",
                is_final=True,
                start_ms=2000,
                language="en",
            ),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(False, "Mr. Smith")],
            [(True, "Mr. Smith went home.")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_english_abbreviation_waits err: {err}"


def test_deferred_tokens_flushed_as_final_on_session_finalize(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens("台北选手戴资颖", is_final=True),
            0,
            0,
        )
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [SonioxFinToken("<fin>", True)],
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(False, "台北选手戴资颖")],
            [(True, "台北选手戴资颖")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_deferred_tokens_flushed_on_finalize err: {err}"


def test_non_zh_en_runtime_result_follows_vendor_finality(patch_soniox_ws):
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        await patch_soniox_ws.websocket_client.trigger_open()
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_transcript(
            _tokens(
                "こんにちは。続き",
                is_final=True,
                language="ja",
            ),
            0,
            0,
        )

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws,
        on_connect=custom_connect,
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    tester = SonioxSentenceEndTester(
        expected_batches=[
            [(True, "こんにちは。続き")],
        ],
    )
    tester.set_test_mode_single(
        "soniox_asr_python", json.dumps(_property_json())
    )
    err = tester.run()
    assert err is None, f"test_non_zh_en_runtime_result err: {err}"
