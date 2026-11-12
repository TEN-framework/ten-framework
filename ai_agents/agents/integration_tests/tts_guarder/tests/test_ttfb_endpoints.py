"""Effective endpoint rules of the selected installed extension only."""

import pytest

from .ttfb_reference import CONFIGS, HOOKS, effective_config

CASES = [
    ("bytedance_tts", {"uri": "wss://top.test/ws"}, "wss://top.test/ws"),
    (
        "bytedance_tts",
        {"uri": "wss://top.test/ws", "params": {"uri": "wss://nested.test/ws"}},
        "wss://nested.test/ws",
    ),
    (
        "bytedance_tts",
        {"url": "wss://top.test/ws", "params": {"url": "wss://nested.test/ws"}},
        "wss://top.test/ws",
    ),
    (
        "bytedance_tts_duplex",
        {"api_url": "wss://top.test/ws", "params": {"api_url": "wss://nested.test/ws"}},
        "wss://nested.test/ws",
    ),
    (
        "minimax_tts_websocket",
        {"url": "wss://top.test/ws", "params": {"url": "wss://nested.test/ws"}},
        "wss://top.test/ws",
    ),
    (
        "minimax_tts_websocket_duplex",
        {"url": "wss://top.test/ws", "params": {"url": "wss://nested.test/ws"}},
        "wss://nested.test/ws",
    ),
    (
        "soul_tts_python",
        {"url": "wss://top.test/ws", "params": {"url": "wss://nested.test/ws"}},
        "wss://nested.test/ws",
    ),
    (
        "soul_tts_python",
        {"params": {"url": "${env:TTFB_ENDPOINT|}"}},
        "wss://env.test/ws",
    ),
    (
        "speko_tts2_python",
        {"params": {"api_key": "k", "base_url": "https://r.test/edge/"}},
        "wss://r.test/edge/v1/tts/stream",
    ),
    (
        "speko_tts2_python",
        {"params": {"api_key": "k", "base_url": "http://127.0.0.1:8080"}},
        "ws://127.0.0.1:8080/v1/tts/stream",
    ),
]

# Normalization of these extensions also validates a required credential.
MINIMAL_CONFIGS = {"speko_tts2_python": {"params": {"api_key": "k"}}}


@pytest.mark.parametrize("vendor,config,expected", CASES)
def test_selected_extension_endpoint_precedence(
    extension_name, monkeypatch, vendor, config, expected
):
    if vendor != extension_name:
        pytest.skip("Endpoint contract belongs to a different extension")
    monkeypatch.setenv("TTFB_ENDPOINT", "wss://env.test/ws")
    _, _, url = effective_config(vendor, config)
    assert url == expected


def test_selected_extension_default_endpoint(extension_name):
    if extension_name not in CONFIGS:
        pytest.skip("No TTFB endpoint registered for this extension")
    _, normalized, url = effective_config(
        extension_name, MINIMAL_CONFIGS.get(extension_name, {})
    )
    if extension_name == "elevenlabs_multiple_context_tts_python":
        url = url or normalized.params.get("base_url") or HOOKS[extension_name][3]
    assert url.startswith("wss://")
