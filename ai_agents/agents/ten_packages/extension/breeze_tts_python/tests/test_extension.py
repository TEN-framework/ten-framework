"""Adapter tests: the extension wires config, client and reported rate."""

from __future__ import annotations

import asyncio
import json

from breeze_tts import SAMPLE_RATE, BreezeTTSClient
from extension import BreezeTTSExtension


def make_extension() -> BreezeTTSExtension:
    return BreezeTTSExtension("breeze_tts_python")


def config_json(**params):
    return json.dumps({"dump": False, "params": {"api_key": "brz_test", "voice_id": "voc_test", **params}})


def test_vendor_and_default_rate():
    extension = make_extension()
    assert extension.vendor() == "breeze"
    assert extension.synthesize_audio_sample_rate() == SAMPLE_RATE


def test_create_config_normalises_rate_and_reports_it():
    extension = make_extension()
    config = asyncio.run(extension.create_config(config_json(sample_rate=32000)))

    assert config.params.sample_rate == SAMPLE_RATE
    assert extension.synthesize_audio_sample_rate() == SAMPLE_RATE


def test_create_config_keeps_supported_rate():
    extension = make_extension()
    asyncio.run(extension.create_config(config_json(sample_rate=16000)))

    assert extension.synthesize_audio_sample_rate() == 16000


def test_create_client_uses_the_extension_config():
    extension = make_extension()
    config = asyncio.run(extension.create_config(config_json()))
    client = asyncio.run(extension.create_client(config, object()))

    assert isinstance(client, BreezeTTSClient)
    assert client.config is config
