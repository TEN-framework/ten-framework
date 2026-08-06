#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
# conftest.py — stub out ten_runtime and ten_ai_base before the extension
# package is imported, so that unit tests for pure-Python helpers can run
# without a full TEN runtime installation.
#
import sys
import types
from unittest.mock import MagicMock


def _make_mock_module(name: str) -> types.ModuleType:
    mod = types.ModuleType(name)
    mod.__spec__ = None  # type: ignore[assignment]
    mod.__getattr__ = lambda attr: MagicMock()  # type: ignore[method-assign]
    return mod


_STUB_MODULES = [
    "ten_runtime",
    "ten_runtime.async_ten_env",
    "ten_ai_base",
    "ten_ai_base.llm",
    "ten_ai_base.llm2",
    "ten_ai_base.struct",
    "ten_ai_base.types",
    "ten_ai_base.config",
    "ten_ai_base.const",
    "ten_ai_base.helper",
    "ten_ai_base.message",
    "PIL",
    "openai",
    "requests",
]

for _name in _STUB_MODULES:
    if _name not in sys.modules:
        sys.modules[_name] = _make_mock_module(_name)

_ten_runtime = sys.modules["ten_runtime"]
for _attr in (
    "Addon",
    "AsyncExtension",
    "AsyncTenEnv",
    "Cmd",
    "CmdResult",
    "Data",
    "StatusCode",
    "TenEnv",
    "register_addon_as_extension",
):
    setattr(_ten_runtime, _attr, MagicMock())

# ten_ai_base.config.BaseConfig must be a real class so grokConfig can
# inherit from it without errors.
import dataclasses


@dataclasses.dataclass
class _BaseConfig:
    @classmethod
    async def create_async(cls, ten_env=None):
        return cls()


sys.modules["ten_ai_base.config"].BaseConfig = _BaseConfig
