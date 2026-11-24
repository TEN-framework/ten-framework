#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import copy
import json
import os
import sys
import types
from unittest.mock import AsyncMock, MagicMock, patch

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
from bytedance_llm_based_asr.volcengine_asr_client import ASRResponse


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
    env.log_debug = MagicMock()
    env.log_error = MagicMock()
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


async def _start_mock_connection(ext):
    ext.config.params.update(
        {"auth_method": "api_key", "api_key": "test-api-key"}
    )
    with patch.object(
        extension_module.VolcengineASRClient, "connect", new=AsyncMock()
    ):
        await ext.start_connection()
    client = ext.client
    assert client is not None
    client.connected = True
    await client._invoke_callback(client.connected_callback)
    return client


@pytest.mark.asyncio
async def test_old_callbacks_preserve_new_connection_turn(mock_ten_env):
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.on_connected = AsyncMock()
    ext.on_disconnected = AsyncMock()
    old_client = await _start_mock_connection(ext)
    await old_client._handle_response(ASRResponse(text="old speech"))
    old_id = ext.uuid

    await ext.stop_connection()
    assert ext.uuid != old_id
    new_client = await _start_mock_connection(ext)
    await new_client._handle_response(ASRResponse(text="new speech"))
    pending = copy.deepcopy(ext._pending_non_final_result)
    new_id = ext.uuid
    mock_ten_env.send_data.reset_mock()
    ext.on_connected.reset_mock()
    ext.on_disconnected.reset_mock()

    await old_client._notify_disconnected(1000, "late close")
    await old_client._handle_response(ASRResponse(text="late old speech"))
    await old_client._invoke_callback(old_client.connected_callback)
    with patch.object(extension_module.asyncio, "create_task") as create_task:
        old_client._handle_connection_error_callback(RuntimeError("late"))
        old_client._handle_asr_error_callback(RuntimeError("late"))
        create_task.assert_not_called()

    assert ext.client is new_client
    assert ext.is_connected()
    assert ext.uuid == new_id
    assert ext._pending_non_final_result == pending
    mock_ten_env.send_data.assert_not_awaited()
    ext.on_connected.assert_not_awaited()
    ext.on_disconnected.assert_not_awaited()

    await new_client._notify_disconnected(1000, "current close")
    data = mock_ten_env.send_data.await_args.args[0]
    payload, error = data.get_property_to_json()
    assert error is None
    result = json.loads(payload)
    assert result["id"] == new_id
    assert result["text"] == "new speech"
    assert result["final"] is True
    ext.on_disconnected.assert_awaited_once()


@pytest.mark.asyncio
@pytest.mark.parametrize("callback", ["result", "disconnected"])
async def test_old_callback_rechecks_client_after_result_lock(
    mock_ten_env, callback
):
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.on_connected = AsyncMock()
    ext.on_disconnected = AsyncMock()
    old_client = await _start_mock_connection(ext)
    await old_client._handle_response(ASRResponse(text="speech"))
    pending = copy.deepcopy(ext._pending_non_final_result)
    turn_id = ext.uuid

    async with ext._asr_result_lock:
        if callback == "result":
            coro = old_client._handle_response(ASRResponse(text="late speech"))
        else:
            coro = old_client._notify_disconnected(1000, "late close")
        task = asyncio.create_task(coro)
        await asyncio.sleep(0)
        assert not task.done()
        new_client = await _start_mock_connection(ext)
        mock_ten_env.send_data.reset_mock()

    await asyncio.wait_for(task, timeout=1)

    assert ext.client is new_client
    assert ext.is_connected()
    assert ext.uuid == turn_id
    assert ext._pending_non_final_result == pending
    mock_ten_env.send_data.assert_not_awaited()
    ext.on_disconnected.assert_not_awaited()


@pytest.mark.asyncio
async def test_disconnect_rechecks_client_after_fallback_send(mock_ten_env):
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.on_connected = AsyncMock()
    ext.on_disconnected = AsyncMock()
    old_client = await _start_mock_connection(ext)
    await old_client._handle_response(ASRResponse(text="old speech"))
    send_started = asyncio.Event()
    allow_send = asyncio.Event()

    async def send_data(_data):
        send_started.set()
        await allow_send.wait()

    mock_ten_env.send_data.side_effect = send_data
    task = asyncio.create_task(
        old_client._notify_disconnected(1000, "old close")
    )
    await asyncio.wait_for(send_started.wait(), timeout=1)
    mock_ten_env.send_data.side_effect = None
    new_client = await _start_mock_connection(ext)
    allow_send.set()
    await asyncio.wait_for(task, timeout=1)

    assert ext.client is new_client
    assert ext.is_connected()
    ext.on_disconnected.assert_not_awaited()


@pytest.mark.asyncio
async def test_old_error_task_does_not_report_or_reconnect(mock_ten_env):
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    ext.config = _minimal_config()
    ext.on_connected = AsyncMock()
    old_client = await _start_mock_connection(ext)
    ext.send_asr_error = AsyncMock()
    ext._handle_reconnect = AsyncMock()

    task = asyncio.create_task(
        ext._on_asr_error(1000, "old failure", client=old_client)
    )
    ext.client = MagicMock()
    await asyncio.wait_for(task, timeout=1)

    ext.send_asr_error.assert_not_awaited()
    ext._handle_reconnect.assert_not_awaited()


@pytest.mark.asyncio
async def test_old_reconnect_rechecks_client_after_swap_lock(mock_ten_env):
    ext = BytedanceASRLLMExtension("t")
    ext.ten_env = mock_ten_env
    old_client = MagicMock()
    ext.client = old_client
    ext.min_retry_delay = 0
    ext.stop_connection = AsyncMock()
    ext.start_connection = AsyncMock()

    async with ext._swap_lock:
        task = asyncio.create_task(ext._handle_reconnect(client=old_client))
        await asyncio.sleep(0)
        assert not task.done()
        ext.client = MagicMock()

    await asyncio.wait_for(task, timeout=1)
    ext.stop_connection.assert_not_awaited()
    ext.start_connection.assert_not_awaited()


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
