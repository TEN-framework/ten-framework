"""Test bootstrap.

The TEN runtime packages (``ten_ai_base``, ``ten_runtime``) ship inside the
``ten_agent_build`` image, not on PyPI, so local runs get a minimal stand-in.
In CI both packages import normally and the stand-in is never installed.
"""

from __future__ import annotations

import enum
import sys
import types
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))


def _install_stub() -> None:
    try:
        import ten_ai_base  # noqa: F401

        return
    except ImportError:
        pass

    base = types.ModuleType("ten_ai_base")
    base.__path__ = []  # type: ignore[attr-defined]

    struct = types.ModuleType("ten_ai_base.struct")

    class TTS2HttpResponseEventType(enum.Enum):
        RESPONSE = "response"
        END = "end"
        FLUSH = "flush"
        ERROR = "error"
        INVALID_KEY_ERROR = "invalid_key_error"

    struct.TTS2HttpResponseEventType = (  # type: ignore[attr-defined]
        TTS2HttpResponseEventType
    )

    pydantic = __import__("pydantic")
    http = types.ModuleType("ten_ai_base.tts2_http")

    class AsyncTTS2HttpConfig(pydantic.BaseModel):  # type: ignore[misc]
        pass

    class AsyncTTS2HttpClient:  # noqa: D401 - stand-in
        def __init__(self) -> None:
            pass

    class AsyncTTS2HttpExtension:  # noqa: D401 - stand-in
        def __init__(self, name: str) -> None:
            self.name = name

    http.AsyncTTS2HttpConfig = (  # type: ignore[attr-defined]
        AsyncTTS2HttpConfig
    )
    http.AsyncTTS2HttpClient = (  # type: ignore[attr-defined]
        AsyncTTS2HttpClient
    )
    http.AsyncTTS2HttpExtension = (  # type: ignore[attr-defined]
        AsyncTTS2HttpExtension
    )

    utils = types.ModuleType("ten_ai_base.utils")

    def _encrypt(value):
        return "***" if value else ""

    utils.encrypt = _encrypt  # type: ignore[attr-defined]

    base.struct = struct  # type: ignore[attr-defined]
    base.tts2_http = http  # type: ignore[attr-defined]
    base.utils = utils  # type: ignore[attr-defined]

    sys.modules["ten_ai_base"] = base
    sys.modules["ten_ai_base.struct"] = struct
    sys.modules["ten_ai_base.tts2_http"] = http
    sys.modules["ten_ai_base.utils"] = utils

    runtime = types.ModuleType("ten_runtime")
    runtime.AsyncTenEnv = object  # type: ignore[attr-defined]
    sys.modules["ten_runtime"] = runtime


_install_stub()
