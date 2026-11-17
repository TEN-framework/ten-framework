#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import copy
import os
import sys
import types
from unittest.mock import AsyncMock, MagicMock

import pytest

extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

package = types.ModuleType("bytedance_llm_based_asr")
package.__path__ = [extension_dir]
sys.modules["bytedance_llm_based_asr"] = package

from bytedance_llm_based_asr import config as config_module

sys.modules["bytedance_llm_based_asr.config"] = config_module

from bytedance_llm_based_asr import extension as extension_module

sys.modules["bytedance_llm_based_asr.extension"] = extension_module

from bytedance_llm_based_asr.config import BytedanceASRLLMConfig
from bytedance_llm_based_asr.extension import (
    BytedanceASRLLMExtension,
    _deep_merge_dict,
    _strip_empty_request_corpus_context,
)


def _minimal_config(
    *, omit_empty_text_results: bool | None = None
) -> BytedanceASRLLMConfig:
    params: dict = {
        "audio": {"rate": 16000},
        "request": {"model_name": "bigmodel"},
    }
    if omit_empty_text_results is not None:
        params["omit_empty_text_results"] = omit_empty_text_results
    return BytedanceASRLLMConfig.model_validate({"params": params})


def test_get_omit_empty_text_results_defaults_false() -> None:
    cfg = _minimal_config()
    assert cfg.get_omit_empty_text_results() is False


def test_get_omit_empty_text_results_true_when_set() -> None:
    cfg = _minimal_config(omit_empty_text_results=True)
    assert cfg.get_omit_empty_text_results() is True


def test_deep_merge_dict_nested() -> None:
    target: dict = {"a": {"b": 1}, "c": 2}
    _deep_merge_dict(target, {"a": {"b": 2, "d": 3}})
    assert target == {"a": {"b": 2, "d": 3}, "c": 2}


def test_deep_merge_dict_non_dict_patch_replaces_dict() -> None:
    target: dict = {"a": {"b": 1}}
    _deep_merge_dict(target, {"a": "scalar"})
    assert target == {"a": "scalar"}


def test_deep_merge_dict_dict_patch_replaces_non_dict() -> None:
    target: dict = {"a": 1}
    _deep_merge_dict(target, {"a": {"nested": True}})
    assert target == {"a": {"nested": True}}


def test_strip_empty_context_removes_empty_string() -> None:
    params = {
        "request": {"corpus": {"context": "", "other": 1}},
    }
    _strip_empty_request_corpus_context(params)
    assert params["request"]["corpus"] == {"other": 1}


def test_strip_empty_context_keeps_non_empty_string() -> None:
    params = {"request": {"corpus": {"context": "hello"}}}
    _strip_empty_request_corpus_context(params)
    assert params["request"]["corpus"]["context"] == "hello"


def test_strip_empty_context_missing_context_no_op() -> None:
    params = {"request": {"corpus": {"other": 1}}}
    before = copy.deepcopy(params)
    _strip_empty_request_corpus_context(params)
    assert params == before


def test_strip_empty_context_request_not_dict() -> None:
    params = {"request": "bad"}
    before = copy.deepcopy(params)
    _strip_empty_request_corpus_context(params)
    assert params == before


def test_strip_empty_context_corpus_not_dict() -> None:
    params = {"request": {"corpus": "bad"}}
    before = copy.deepcopy(params)
    _strip_empty_request_corpus_context(params)
    assert params == before


@pytest.fixture
def mock_ten_env():
    env = AsyncMock()
    env.log_info = MagicMock()
    return env


@pytest.mark.asyncio
async def test_send_asr_result_from_text_omit_on_skips_empty(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config(omit_empty_text_results=True)
    ext.send_asr_result = AsyncMock()

    sent = await ext._send_asr_result_from_text(
        text="   ",
        is_final=True,
        start_ms=0,
        duration_ms=1,
        language="zh-CN",
        metadata={},
    )
    assert sent is False
    ext.send_asr_result.assert_not_awaited()


@pytest.mark.asyncio
async def test_send_asr_result_from_text_omit_on_sends_non_empty(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config(omit_empty_text_results=True)
    ext.send_asr_result = AsyncMock()

    sent = await ext._send_asr_result_from_text(
        text="hi",
        is_final=True,
        start_ms=0,
        duration_ms=1,
        language="zh-CN",
        metadata={},
    )
    assert sent is True
    ext.send_asr_result.assert_awaited_once()


@pytest.mark.asyncio
async def test_send_asr_result_from_text_omit_off_sends_whitespace_only(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config(omit_empty_text_results=False)
    ext.send_asr_result = AsyncMock()

    sent = await ext._send_asr_result_from_text(
        text="  ",
        is_final=False,
        start_ms=0,
        duration_ms=1,
        language="zh-CN",
        metadata={},
    )
    assert sent is True
    ext.send_asr_result.assert_awaited_once()


@pytest.mark.asyncio
async def test_send_asr_result_from_text_no_config_sends(mock_ten_env) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = None
    ext.send_asr_result = AsyncMock()

    sent = await ext._send_asr_result_from_text(
        text="",
        is_final=True,
        start_ms=0,
        duration_ms=1,
        language="zh-CN",
        metadata={},
    )
    assert sent is True
    ext.send_asr_result.assert_awaited_once()


async def _send_result_and_advance_uuid(ext, result) -> None:
    result.id = ext.uuid
    if result.final:
        ext.uuid = "next-turn"


def _send_result_mock(ext) -> AsyncMock:
    async def send_result(result) -> None:
        await _send_result_and_advance_uuid(ext, result)

    return AsyncMock(side_effect=send_result)


@pytest.mark.asyncio
async def test_non_final_result_cache_tracks_latest_result_for_same_id(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.uuid = "turn-1"
    ext.send_asr_result = _send_result_mock(ext)

    await ext._send_asr_result_from_text(
        text="hello",
        is_final=False,
        start_ms=0,
        duration_ms=100,
        language="zh-CN",
        metadata={},
    )
    await ext._send_asr_result_from_text(
        text="hello world",
        is_final=False,
        start_ms=0,
        duration_ms=200,
        language="zh-CN",
        metadata={},
    )

    assert ext._pending_non_final_result is not None
    assert ext._pending_non_final_result.id == "turn-1"
    assert ext._pending_non_final_result.text == "hello world"
    assert ext._pending_non_final_result.final is False


@pytest.mark.asyncio
async def test_final_result_clears_non_final_cache_for_same_id(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.uuid = "turn-1"
    ext.send_asr_result = _send_result_mock(ext)

    await ext._send_asr_result_from_text(
        text="hello",
        is_final=False,
        start_ms=0,
        duration_ms=100,
        language="zh-CN",
        metadata={},
    )
    await ext._send_asr_result_from_text(
        text="hello world",
        is_final=True,
        start_ms=0,
        duration_ms=200,
        language="zh-CN",
        metadata={},
    )

    assert ext._pending_non_final_result is None


@pytest.mark.asyncio
async def test_disconnect_sends_cached_non_final_as_same_id_final(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.uuid = "turn-1"
    ext.on_disconnected = AsyncMock()
    ext.send_asr_result = _send_result_mock(ext)

    await ext._send_asr_result_from_text(
        text="hello world",
        is_final=False,
        start_ms=10,
        duration_ms=200,
        language="zh-CN",
        metadata={"asr_info": {"vendor": "bytedance_bigmodel"}},
    )
    await ext._on_disconnected()

    assert ext.send_asr_result.await_count == 2
    fallback = ext.send_asr_result.await_args_list[1].args[0]
    assert fallback.id == "turn-1"
    assert fallback.text == "hello world"
    assert fallback.final is True
    assert fallback.start_ms == 10
    assert fallback.duration_ms == 200
    assert ext._pending_non_final_result is None

    await ext._on_disconnected()
    assert ext.send_asr_result.await_count == 2


@pytest.mark.asyncio
async def test_run_update_configs_returns_false_when_config_missing(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = None
    ext.stop_connection = AsyncMock()
    ext.start_connection = AsyncMock()

    ok, msg = await ext._run_update_configs({"params": {"x": 1}})
    assert ok is False
    assert msg == "config not loaded"
    ext.stop_connection.assert_not_awaited()


@pytest.mark.asyncio
async def test_run_update_configs_merges_and_strips_empty_context(
    mock_ten_env,
) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.config.params["request"]["corpus"] = {"context": "old"}
    ext.stop_connection = AsyncMock()
    ext.start_connection = AsyncMock()

    ok, msg = await ext._run_update_configs(
        {
            "params": {
                "request": {
                    "corpus": {"context": ""},
                    "enable_nonstream": False,
                }
            }
        }
    )
    assert ok is True
    assert msg == ""
    req = ext.config.params["request"]
    assert "context" not in req.get("corpus", {})
    assert req["enable_nonstream"] is False
    ext.stop_connection.assert_awaited_once()
    ext.start_connection.assert_awaited_once()


@pytest.mark.asyncio
async def test_run_update_configs_sets_trimmed_api_url(mock_ten_env) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.stop_connection = AsyncMock()
    ext.start_connection = AsyncMock()

    ok, _ = await ext._run_update_configs({"url": "  wss://example.test/asr  "})
    assert ok is True
    assert ext.config.params["api_url"] == "wss://example.test/asr"


@pytest.mark.asyncio
async def test_run_update_configs_dump_flag(mock_ten_env) -> None:
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.config.dump = False
    ext.stop_connection = AsyncMock()
    ext.start_connection = AsyncMock()

    ok, _ = await ext._run_update_configs({"dump": True})
    assert ok is True
    assert ext.config.dump is True
