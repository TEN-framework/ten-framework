"""Tests for _apply_string_field_under_params nested path writes."""

import os
import sys
import types

extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

package = types.ModuleType("bytedance_llm_based_asr")
package.__path__ = [extension_dir]
sys.modules["bytedance_llm_based_asr"] = package

from bytedance_llm_based_asr import config as config_module  # noqa: E402

sys.modules["bytedance_llm_based_asr.config"] = config_module

from bytedance_llm_based_asr import extension as extension_module  # noqa: E402

sys.modules["bytedance_llm_based_asr.extension"] = extension_module

from bytedance_llm_based_asr.extension import (  # noqa: E402
    _apply_string_field_under_params,
)


def test_creates_missing_intermediate_dicts():
    params: dict = {}
    ok, err = _apply_string_field_under_params(
        params, "request.corpus.context", '{"x":1}'
    )
    assert ok and err == ""
    assert params["request"]["corpus"]["context"] == '{"x":1}'


def test_conflict_when_intermediate_is_non_dict_no_mutation():
    params = {"request": "not-a-dict"}
    ok, err = _apply_string_field_under_params(
        params, "request.corpus.context", "v"
    )
    assert not ok
    assert err.startswith("path_conflict_non_dict_at:")
    assert params == {"request": "not-a-dict"}


def test_clear_terminal_key():
    params = {"a": {"b": "x"}}
    ok, err = _apply_string_field_under_params(params, "a.b", "")
    assert ok and err == ""
    assert params == {"a": {}}
