#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from __future__ import annotations

import asyncio
import json

import httpx
import pytest

from generic_video_python.generic import AgoraGenericRecorder


class FakeTenEnv:
    def __init__(self):
        self.infos: list[str] = []
        self.errors: list[str] = []
        self.warns: list[str] = []
        self.debugs: list[str] = []

    def log_info(self, msg: str, **_kwargs) -> None:
        self.infos.append(msg)

    def log_error(self, msg: str, **_kwargs) -> None:
        self.errors.append(msg)

    def log_warn(self, msg: str, **_kwargs) -> None:
        self.warns.append(msg)

    def log_debug(self, msg: str, **_kwargs) -> None:
        self.debugs.append(msg)

    async def send_data(self, _data) -> None:
        return None


class RecordingWebSocket:
    def __init__(self):
        self.messages: list[dict] = []
        self.state = type("OpenState", (), {"name": "OPEN"})()

    async def send(self, payload: str) -> None:
        self.messages.append(json.loads(payload))


def create_recorder(
    *,
    http_client: httpx.AsyncClient | None = None,
    session_cache_path: str | None = None,
) -> AgoraGenericRecorder:
    return AgoraGenericRecorder(
        app_id="appid",
        app_cert="",
        api_key="api-key",
        channel_name="room-a",
        avatar_uid=321,
        ten_env=FakeTenEnv(),
        avatar_id="avatar-1",
        quality="high",
        version="v1",
        video_encoding="H264",
        area="NORTH_AMERICA",
        enable_string_uid=False,
        start_endpoint="https://example.test/session/start",
        stop_endpoint="https://example.test/session/stop",
        activity_idle_timeout=120,
        http_client=http_client,
        session_cache_path=session_cache_path,
    )


def test_start_and_init_payloads_match_contract():
    recorder = create_recorder()
    recorder.session_id = "session-1"

    start_payload = recorder._build_start_payload()
    init_payload = recorder._build_init_payload()

    assert start_payload["area"] == "NORTH_AMERICA"
    assert start_payload["agora_settings"]["channel"] == "room-a"
    assert start_payload["agora_settings"]["uid"] == "321"
    assert init_payload["command"] == "init"
    assert init_payload["session_id"] == "session-1"
    assert init_payload["area"] == "NORTH_AMERICA"


def test_stop_payload_requires_session_token():
    recorder = create_recorder()

    with pytest.raises(ValueError):
        recorder._build_stop_payload("session-1")


def test_stop_payload_includes_session_token():
    recorder = create_recorder()
    payload = recorder._build_stop_payload(
        "session-1", session_token="session-token"
    )

    assert payload == {
        "session_id": "session-1",
        "session_token": "session-token",
    }


def test_cache_path_is_scoped_per_recorder():
    recorder_a = create_recorder()
    recorder_b = AgoraGenericRecorder(
        app_id="appid",
        app_cert="",
        api_key="api-key",
        channel_name="room-b",
        avatar_uid=321,
        ten_env=FakeTenEnv(),
        avatar_id="avatar-1",
        quality="high",
        version="v1",
        video_encoding="H264",
        area="GLOBAL",
        enable_string_uid=False,
        start_endpoint="https://example.test/session/start",
        stop_endpoint="https://example.test/session/stop",
        activity_idle_timeout=120,
        http_client=httpx.AsyncClient(
            transport=httpx.MockTransport(
                lambda request: httpx.Response(200, json={})
            )
        ),
    )

    assert recorder_a.session_cache_path != recorder_b.session_cache_path

    asyncio.run(recorder_a.http_client.aclose())
    asyncio.run(recorder_b.http_client.aclose())


def test_create_session_sends_area_and_parses_response():
    captured = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["json"] = json.loads(request.content.decode())
        captured["headers"] = dict(request.headers)
        return httpx.Response(
            200,
            json={
                "session_id": "session-1",
                "websocket_address": "ws://example.test/ws",
                "session_token": "token-1",
            },
        )

    async def _run():
        client = httpx.AsyncClient(transport=httpx.MockTransport(handler))
        recorder = create_recorder(http_client=client)
        await recorder._create_session()
        await client.aclose()

        assert captured["json"]["area"] == "NORTH_AMERICA"
        assert captured["json"]["activity_idle_timeout"] == 120
        assert captured["headers"]["x-api-key"] == "api-key"
        assert recorder.session_id == "session-1"
        assert recorder.realtime_endpoint == "ws://example.test/ws"
        assert recorder.session_token == "token-1"

    asyncio.run(_run())


def test_stop_session_sends_session_token_in_body(tmp_path):
    captured = {}

    def handler(request: httpx.Request) -> httpx.Response:
        captured["method"] = request.method
        captured["json"] = json.loads(request.content.decode())
        return httpx.Response(200, json={"status": "success"})

    async def _run():
        client = httpx.AsyncClient(transport=httpx.MockTransport(handler))
        recorder = create_recorder(
            http_client=client,
            session_cache_path=str(tmp_path / "session.json"),
        )
        recorder.session_token = "token-1"
        recorder._save_session("session-1", "token-1")
        await recorder._stop_session("session-1")
        await client.aclose()

        assert captured["method"] == "DELETE"
        assert captured["json"] == {
            "session_id": "session-1",
            "session_token": "token-1",
        }
        assert not (tmp_path / "session.json").exists()

    asyncio.run(_run())


def test_send_interrupt_voice_end_and_voice_messages():
    async def _run():
        recorder = create_recorder()
        recorder.websocket = RecordingWebSocket()

        await recorder.send("YWJj", sample_rate=44100)
        await recorder.interrupt()
        await recorder.send_voice_end()
        messages = recorder.websocket.messages

        assert messages[0]["command"] == "voice"
        assert messages[0]["sampleRate"] == 44100
        assert messages[0]["encoding"] == "PCM16"
        assert messages[1]["command"] == "voice_interrupt"
        assert messages[2]["command"] == "voice_end"

        await recorder.http_client.aclose()

    asyncio.run(_run())
