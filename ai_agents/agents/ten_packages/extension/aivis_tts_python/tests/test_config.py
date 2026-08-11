"""Unit tests for AivisTTSConfig (no TEN runtime required)."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import MagicMock

# Stub TEN packages so config can be imported standalone.
sys.modules.setdefault("ten_ai_base", MagicMock())
sys.modules.setdefault("ten_ai_base.tts2_http", MagicMock())
sys.modules.setdefault("ten_ai_base.utils", MagicMock())

# Provide a minimal AsyncTTS2HttpConfig stand-in before importing config.
import types

tts2_http = types.ModuleType("ten_ai_base.tts2_http")


class _AsyncTTS2HttpConfig:
    pass


tts2_http.AsyncTTS2HttpConfig = _AsyncTTS2HttpConfig
sys.modules["ten_ai_base.tts2_http"] = tts2_http

utils_mod = types.ModuleType("ten_ai_base.utils")
utils_mod.encrypt = lambda x: f"***{str(x)[-4:]}"
sys.modules["ten_ai_base.utils"] = utils_mod

ten_ai_base = types.ModuleType("ten_ai_base")
ten_ai_base.utils = utils_mod
sys.modules["ten_ai_base"] = ten_ai_base

# Make pydantic BaseModel work with our fake parent via a real pydantic model.
from pydantic import BaseModel, Field
from typing import Any


class AsyncTTS2HttpConfig(BaseModel):
    dump: bool = False
    dump_path: str = ""
    params: dict[str, Any] = Field(default_factory=dict)


tts2_http.AsyncTTS2HttpConfig = AsyncTTS2HttpConfig

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

# Patch config module's import target, then load.
import importlib.util

spec = importlib.util.spec_from_file_location(
    "aivis_config", ROOT / "config.py"
)
assert spec and spec.loader
mod = importlib.util.module_from_spec(spec)
# Ensure the module sees our patched AsyncTTS2HttpConfig
import ten_ai_base.tts2_http as patched  # noqa: E402

patched.AsyncTTS2HttpConfig = AsyncTTS2HttpConfig
spec.loader.exec_module(mod)

AivisTTSConfig = mod.AivisTTSConfig


class TestAivisTTSConfig(unittest.TestCase):
    def test_validate_requires_api_key(self):
        cfg = AivisTTSConfig(params={"model_uuid": "abc"})
        with self.assertRaises(ValueError):
            cfg.validate()

    def test_update_params_forces_wav_and_defaults(self):
        cfg = AivisTTSConfig(
            params={
                "api_key": "secret",
                "model_uuid": "abc",
                "sample_rate": 24000,
                "output_format": "mp3",
            }
        )
        cfg.update_params()
        self.assertEqual(cfg.params["output_format"], "wav")
        self.assertEqual(cfg.params["output_sampling_rate"], 24000)
        self.assertEqual(cfg.params["output_audio_channels"], "mono")
        self.assertEqual(cfg.params["language"], "ja")

    def test_request_body_strips_client_keys(self):
        cfg = AivisTTSConfig(
            params={
                "api_key": "secret",
                "model_uuid": "abc",
                "base_url": "https://example.test",
            }
        )
        cfg.update_params()
        body = cfg.request_body("こんにちは")
        self.assertEqual(body["text"], "こんにちは")
        self.assertEqual(body["model_uuid"], "abc")
        self.assertNotIn("api_key", body)
        self.assertNotIn("base_url", body)
        self.assertEqual(body["output_format"], "wav")

    def test_synthesize_url(self):
        cfg = AivisTTSConfig(
            params={
                "api_key": "secret",
                "model_uuid": "abc",
                "base_url": "https://api.example.com/",
            }
        )
        cfg.update_params()
        self.assertEqual(
            cfg.synthesize_url(),
            "https://api.example.com/v1/tts/synthesize",
        )


if __name__ == "__main__":
    unittest.main()
