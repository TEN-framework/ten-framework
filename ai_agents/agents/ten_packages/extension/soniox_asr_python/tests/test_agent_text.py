# ABOUTME: Tests update_configs to Soniox agent_text WebSocket forwarding.
# ABOUTME: Covers config gating, chunking, and no reconnect side effects.
from __future__ import annotations

import json
from unittest.mock import AsyncMock, MagicMock

import pytest

from ..agent_text import build_agent_text_frame, chunk_agent_text, parse_agent_text_update
from ..config import SonioxASRConfig
from ..const import AGENT_TEXT_MAX_CHUNK_CHARS
from ..extension import SonioxASRExtension
from ..websocket import SonioxWebsocketClient


def test_config_enables_agent_text_for_v6():
    cfg = SonioxASRConfig(
        params={
            "model": "stt-rt-v6",
            "enable_agent_text_input": True,
        }
    )
    assert cfg.supports_agent_text_input() is True


def test_config_skips_agent_text_for_non_v6_model():
    cfg = SonioxASRConfig(
        params={
            "model": "stt-rt-v3",
            "enable_agent_text_input": True,
        }
    )
    assert cfg.supports_agent_text_input() is False


def test_parse_agent_text_update():
    params = {
        "agent_text": {"text": "hello", "end_of_segment": True},
    }
    assert parse_agent_text_update(params) == ("hello", True)
    assert parse_agent_text_update({}) is None


def test_chunk_agent_text_sets_end_only_on_last():
    long_text = "a" * (AGENT_TEXT_MAX_CHUNK_CHARS + 10)
    chunks = chunk_agent_text(long_text, AGENT_TEXT_MAX_CHUNK_CHARS, True)
    assert len(chunks) == 2
    assert chunks[0][1] is False
    assert chunks[1][1] is True


@pytest.mark.asyncio
async def test_websocket_send_agent_text_payload():
    client = SonioxWebsocketClient("wss://example", "{}", "key")
    client._send_queue = AsyncMock()
    await client.send_agent_text("hi", end_of_segment=True)
    expected = json.dumps(
        {"type": "agent_text", "text": "hi", "end_of_segment": True}
    )
    client._send_queue.put.assert_awaited_once_with(expected)


@pytest.mark.asyncio
async def test_extension_forwards_agent_text_when_connected():
    ext = SonioxASRExtension("asr")
    ext.config = SonioxASRConfig(
        params={
            "model": "stt-rt-v6",
            "enable_agent_text_input": True,
            "api_key": "k",
        }
    )
    ext.connected = True
    ext.websocket = MagicMock()
    ext.websocket.send_agent_text = AsyncMock()
    ten_env = MagicMock()
    ten_env.log_warn = MagicMock()
    ten_env.log_debug = MagicMock()

    await ext._forward_agent_text_update(
        ten_env, "River Roast?", end_of_segment=True
    )

    ext.websocket.send_agent_text.assert_awaited_once_with(
        "River Roast?",
        end_of_segment=True,
    )
    ext.websocket.stop.assert_not_called()
    assert ext._needs_reconnect is False


def test_build_agent_text_frame():
    assert build_agent_text_frame("x", False) == {
        "type": "agent_text",
        "text": "x",
        "end_of_segment": False,
    }
