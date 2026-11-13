#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
"""Pure fixtures shared by the unit tests.

Nothing here imports ``ten_runtime``, ``ten_ai_base`` or ``spatialreal`` so
these tests run on any laptop with pytest. Runtime tests that need the TEN
runtime live under ``tests/runtime/`` behind an ``importorskip`` guard.

The pure modules import their siblings relatively (``from .audio import``),
the way the TEN addon loader imports the extension as a package. The tests
reach them as ``spatialreal_avatar_python.<module>`` through a stub package
whose ``__path__`` is the extension directory, so the real ``__init__``
(which imports ``ten_runtime``) never runs here.

The coroutine tests (``@pytest.mark.asyncio``) run under pytest-asyncio when
it is installed. The TEN CI image has pytest without pytest-asyncio, where
pytest 8 skips coroutine tests and pytest 9 fails them, so without the plugin
``pytest_pyfunc_call`` below runs each coroutine on a fresh event loop. All
fixtures are synchronous, which is what keeps this fallback small.
"""

import asyncio
import inspect
import pathlib
import sys
import types

import pytest

_pkg = types.ModuleType("spatialreal_avatar_python")
_pkg.__path__ = [str(pathlib.Path(__file__).resolve().parents[1])]
sys.modules.setdefault("spatialreal_avatar_python", _pkg)


def _has_pytest_asyncio(config: pytest.Config) -> bool:
    return config.pluginmanager.hasplugin("asyncio")


def pytest_configure(config: pytest.Config) -> None:
    if not _has_pytest_asyncio(config):
        config.addinivalue_line(
            "markers",
            "asyncio: coroutine test, run on a fresh event loop when "
            "pytest-asyncio is not installed",
        )


@pytest.hookimpl(tryfirst=True)
def pytest_pyfunc_call(pyfuncitem: pytest.Function):
    if _has_pytest_asyncio(pyfuncitem.config):
        return None
    func = pyfuncitem.obj
    if not inspect.iscoroutinefunction(func):
        return None
    names = inspect.signature(func).parameters
    kwargs = {
        n: pyfuncitem.funcargs[n] for n in names if n in pyfuncitem.funcargs
    }
    asyncio.run(func(**kwargs))
    return True


class FakeClock:
    """A clock the test advances by hand; ``now()`` is in seconds."""

    def __init__(self, start_s: float = 1000.0):
        self._now_s = start_s

    def now(self) -> float:
        return self._now_s

    def advance(self, ms: float) -> None:
        self._now_s += ms / 1000.0


@pytest.fixture
def clock() -> FakeClock:
    return FakeClock()
