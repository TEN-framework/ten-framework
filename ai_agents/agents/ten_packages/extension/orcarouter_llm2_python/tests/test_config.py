#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Config tests for the OrcaRouter LLM2 extension.

Importing the extension module requires the TEN AI base package, so these
tests are skipped where it is unavailable (e.g. outside the built framework).
"""
import sys
from pathlib import Path

import pytest

_EXT_DIR = str(Path(__file__).resolve().parents[1])
if _EXT_DIR not in sys.path:
    sys.path.insert(0, _EXT_DIR)

pytest.importorskip("ten_ai_base")

from orcarouter import OrcaRouterLLM2Config  # noqa: E402


def test_default_base_url_and_model():
    cfg = OrcaRouterLLM2Config()
    assert cfg.base_url == "https://api.orcarouter.ai/v1"
    assert cfg.model == "orcarouter/auto"


def test_black_list_params():
    cfg = OrcaRouterLLM2Config()
    for key in ("messages", "tools", "stream", "n", "model"):
        assert cfg.is_black_list_params(key)
    # Routing controls are NOT black-listed: they must pass through to the API.
    assert not cfg.is_black_list_params("models")
    assert not cfg.is_black_list_params("route")


def test_custom_headers_defaults_empty():
    cfg = OrcaRouterLLM2Config()
    assert cfg.custom_headers == {}
