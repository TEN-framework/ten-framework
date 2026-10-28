"""Test config merge logic for _merge_dict and merge_with_dict."""

import json
import sys
import os

# Add project path to sys.path
extension_dir = os.path.join(os.path.dirname(__file__), "..")
sys.path.insert(0, extension_dir)

# Create a mock package structure to allow relative imports
import types
bytedance_tts_duplex_package = types.ModuleType('bytedance_tts_duplex')
bytedance_tts_duplex_package.__path__ = [extension_dir]
sys.modules['bytedance_tts_duplex'] = bytedance_tts_duplex_package

# Now we can import normally
from bytedance_tts_duplex import config as config_module
sys.modules['bytedance_tts_duplex.config'] = config_module

from bytedance_tts_duplex.config import BytedanceTTSDuplexConfig


def test_merge_dict_simple():
    """Test simple dictionary merge without nesting."""
    config = BytedanceTTSDuplexConfig(speaker="test", app_id="test", token="test")
    base = {"a": 1, "b": 2}
    updates = {"b": 3, "c": 4}

    config._merge_dict(base, updates)

    assert base == {"a": 1, "b": 3, "c": 4}


def test_merge_dict_nested():
    """Test deep merge with nested dictionaries."""
    config = BytedanceTTSDuplexConfig(speaker="test", app_id="test", token="test")
    base = {
        "outer": {
            "inner1": 1,
            "inner2": 2
        },
        "other": "value"
    }
    updates = {
        "outer": {
            "inner2": 20,
            "inner3": 3
        }
    }

    config._merge_dict(base, updates)

    assert base == {
        "outer": {
            "inner1": 1,
            "inner2": 20,
            "inner3": 3
        },
        "other": "value"
    }


def test_merge_with_dict_top_level_fields():
    """Test merging top-level config fields."""
    config = BytedanceTTSDuplexConfig(
        app_id="app1",
        token="token1",
        speaker="speaker1",
        dump=False,
    )

    updates = {
        "speaker": "speaker2",
        "dump": True,
    }

    config.merge_with_dict(updates)

    assert config.speaker == "speaker2"
    assert config.dump == True
    assert config.app_id == "app1"  # unchanged


def test_merge_with_dict_params_deep_merge():
    """Test deep merge of params dict."""
    config = BytedanceTTSDuplexConfig(
        app_id="app1",
        token="token1",
        speaker="speaker1",
        params={
            "audio_params": {
                "sample_rate": 16000,
                "vol": 10
            },
            "extra": "value"
        }
    )

    updates = {
        "params": {
            "audio_params": {
                "vol": 20,
                "speed": 10
            }
        }
    }

    config.merge_with_dict(updates)

    # Check deep merge preserved unchanged fields
    assert config.params["audio_params"]["sample_rate"] == 16000
    assert config.params["audio_params"]["vol"] == 20
    assert config.params["audio_params"]["speed"] == 10
    assert config.params["extra"] == "value"


def test_merge_with_dict_update_params_called():
    """Test that update_params() is called to apply params to top-level fields."""
    config = BytedanceTTSDuplexConfig(
        app_id="app1",
        token="token1",
        speaker="old_speaker",
        sample_rate=16000
    )

    updates = {
        "params": {
            "speaker": "new_speaker",
            "audio_params": {
                "sample_rate": 24000
            }
        }
    }

    config.merge_with_dict(updates)

    assert config.speaker == "new_speaker"
    assert config.sample_rate == 24000


def test_update_params_extracts_control_fields_and_defaults_audio_params():
    """Test update_params keeps control fields and audio defaults in sync."""
    config = BytedanceTTSDuplexConfig(
        url="wss://override.example.test/tts",
        sample_rate=16000,
        enable_words=True,
        params={
            "app_id": "app-from-params",
            "api_key": "key-from-params",
            "api_url": "wss://params.example.test/tts",
            "token": "token-from-params",
            "resource_id": "resource-from-params",
            "speaker": "speaker-from-params",
            "model": "model-from-params",
        },
    )

    config.update_params()

    assert config.app_id == "app-from-params"
    assert config.api_key == "key-from-params"
    assert config.api_url == "wss://override.example.test/tts"
    assert config.token == "token-from-params"
    assert config.resource_id == "resource-from-params"
    assert config.speaker == "speaker-from-params"
    assert config.model == "model-from-params"
    assert config.sample_rate == 16000
    assert config.params == {
        "speaker": "speaker-from-params",
        "model": "model-from-params",
        "audio_params": {
            "sample_rate": 16000,
            "format": "pcm",
            "enable_timestamp": True,
            "enable_subtitle": True,
        },
        "additions": '{"cache_config": {"use_cache": false}}',
    }


def test_update_params_preserves_explicit_cache_when_subtitles_enabled():
    config = BytedanceTTSDuplexConfig(
        enable_words=True,
        params={
            "additions": json.dumps({"cache_config": {"use_cache": True}}),
        },
    )

    config.update_params()

    assert json.loads(config.params["additions"])["cache_config"]["use_cache"] is True


def test_update_params_cache_and_subtitle_combinations():
    cases = (
        (False, True, True),
        (False, False, False),
        (True, True, True),
        (True, False, False),
        (True, None, False),
    )

    for enable_words, use_cache, expected_cache in cases:
        additions = (
            "{}"
            if use_cache is None
            else json.dumps({"cache_config": {"use_cache": use_cache}})
        )
        config = BytedanceTTSDuplexConfig(
            enable_words=enable_words,
            params={"additions": additions},
        )

        config.update_params()

        actual = json.loads(config.params["additions"])
        assert actual.get("cache_config", {}).get("use_cache") is expected_cache


def test_merge_with_dict_empty_updates():
    """Test merging with empty updates dict."""
    config = BytedanceTTSDuplexConfig(
        app_id="app1",
        token="token1",
        speaker="speaker1"
    )

    config.merge_with_dict({})

    assert config.speaker == "speaker1"
    assert config.app_id == "app1"


def test_update_params_ignores_non_string_credentials():
    """Invalid credentials do not reach fields or sensitive logging."""
    config = BytedanceTTSDuplexConfig(
        params={"app_id": 123, "token": {"value": "bad"}},
    )

    config.update_params()

    assert config.app_id == ""
    assert config.token == ""
    assert config.to_str(sensitive_handling=True)


def test_update_params_rejects_boolean_sample_rate():
    """Boolean sample rates fall back and are normalized in the request params."""
    for value in (False, True):
        config = BytedanceTTSDuplexConfig(
            params={"audio_params": {"sample_rate": value}},
        )

        config.update_params()

        assert config.sample_rate == 24000
        assert config.params["audio_params"]["sample_rate"] == 24000
