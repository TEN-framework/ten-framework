import asyncio
import json
import uuid

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override

from tencent_asr_client import ResponseData

from .mock import MockClient, patch_tencent_asr_client  # noqa: F401


def _property_json() -> dict:
    return {
        "params": {
            "secretid": "fake_secretid",
            "engine_model_type": "16k_en",
            "voice_format": 1,
            "word_info": 2,
            "appid": "fake_app_id",
            "secretkey": "fake_secret_key",
            "finalize_mode": "mute_pkg",
            "vad_silence_time": 900,
            "log_level": "DEBUG",
        },
    }


class ConnectionStatusExtensionTester(AsyncExtensionTester):
    def __init__(self, timeout_seconds: float = 3.0):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.status_events: list[dict] = []
        self.test_timeout_task: asyncio.Task[None] | None = None
        self.timeout_seconds = timeout_seconds

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        while not self.stopped:
            chunk = b"\x01\x02" * 160
            audio_frame = AudioFrame.create("pcm_frame")
            audio_frame.set_property_from_json(
                "metadata", json.dumps({"session_id": "test_connection_status"})
            )
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    async def timeout_handler(self, ten_env_tester: AsyncTenEnvTester):
        await asyncio.sleep(self.timeout_seconds)
        err = TenError.create(
            error_code=TenErrorCode.ErrorCodeGeneric,
            error_message=(
                f"connection_status_changed not complete, got: {self.status_events}"
            ),
        )
        ten_env_tester.stop_test(err)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )
        self.test_timeout_task = asyncio.create_task(
            self.timeout_handler(ten_env_tester)
        )

    def _validate_connected_payload(self, payload: dict) -> None:
        assert payload["vendor_info"]["vendor"] == "tencent"
        assert payload["metadata"]["vendor_metadata"]["url"] == (
            "wss://asr.cloud.tencent.com/asr/v2"
        )
        assert payload["metadata"]["vendor_metadata"]["model"] == "16k_en"

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "connection_status_changed":
            return

        payload = json.loads(data.get_property_to_json()[0])
        self.status_events.append(payload)
        ten_env_tester.log_info(
            f"connection_status_changed: {payload['last']} -> {payload['current']}"
        )

        if payload["current"] != "connected":
            return

        self._validate_connected_payload(payload)

        current_states = [event["current"] for event in self.status_events]
        assert current_states[0] == "connecting"
        assert "connected" in current_states

        if self.test_timeout_task:
            self.test_timeout_task.cancel()
        ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass
        if self.test_timeout_task:
            self.test_timeout_task.cancel()
            try:
                await self.test_timeout_task
            except asyncio.CancelledError:
                pass


class ReconnectionStatusExtensionTester(ConnectionStatusExtensionTester):
    def __init__(self):
        super().__init__(timeout_seconds=6.0)

    def _reconnection_complete(self) -> bool:
        if not self.status_events:
            return False
        currents = [event["current"] for event in self.status_events]
        transitions = [
            (event["last"], event["current"]) for event in self.status_events
        ]
        return (
            "disconnected" in currents
            and currents.count("connecting") >= 2
            and ("disconnected", "connecting") in transitions
            and currents[-1] == "connected"
        )

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "connection_status_changed":
            return

        payload = json.loads(data.get_property_to_json()[0])
        self.status_events.append(payload)
        ten_env_tester.log_info(
            f"connection_status_changed: {payload['last']} -> {payload['current']}"
        )

        if not self._reconnection_complete():
            return

        self._validate_connected_payload(payload)

        if self.test_timeout_task:
            self.test_timeout_task.cancel()
        ten_env_tester.stop_test()


async def _successful_start(client: MockClient) -> None:
    client.connect_count += 1
    client._is_connected = True
    await client.listener.on_asr_start(
        ResponseData(code=0, message="success", voice_id=str(uuid.uuid4()))
    )


def _assert_reconnection_sequence(status_events: list[dict]) -> None:
    currents = [event["current"] for event in status_events]
    transitions = [(event["last"], event["current"]) for event in status_events]
    assert "disconnected" in currents
    assert currents.count("connecting") >= 2
    assert ("disconnected", "connecting") in transitions
    assert "connected" in currents


def test_connection_status_changed(patch_tencent_asr_client):
    MockClient.start_hook = _successful_start
    try:
        tester = ConnectionStatusExtensionTester()
        tester.set_test_mode_single(
            "tencent_asr_python", json.dumps(_property_json())
        )
        err = tester.run()
        assert err is None, f"test_connection_status_changed err: {err}"
        assert any(
            event["current"] == "connecting" for event in tester.status_events
        )
        assert any(
            event["current"] == "connected" for event in tester.status_events
        )
    finally:
        MockClient.start_hook = None


def test_connection_status_reconnect_on_error(patch_tencent_asr_client):
    async def error_then_success(client: MockClient) -> None:
        client.connect_count += 1
        if client.connect_count == 1:
            await client.listener.on_asr_error(
                ResponseData[str](
                    code=9998,
                    message="error",
                    voice_id="test-voice",
                    result="connection refused",
                ),
                ConnectionRefusedError("connection refused"),
            )
            client._is_running = False
            return
        await _successful_start(client)

    MockClient.start_hook = error_then_success
    try:
        tester = ReconnectionStatusExtensionTester()
        tester.set_test_mode_single(
            "tencent_asr_python", json.dumps(_property_json())
        )
        err = tester.run()
        assert (
            err is None
        ), f"test_connection_status_reconnect_on_error err: {err}"
        _assert_reconnection_sequence(tester.status_events)
    finally:
        MockClient.start_hook = None


def test_connection_status_reconnect_on_close(patch_tencent_asr_client):
    async def close_then_success(client: MockClient) -> None:
        client.connect_count += 1
        if client.connect_count == 1:
            client._is_connected = True
            await client.listener.on_asr_start(
                ResponseData(
                    code=0, message="success", voice_id=str(uuid.uuid4())
                )
            )
            await asyncio.sleep(0.2)
            client._is_connected = False
            await client.listener.on_asr_close(1006, "abnormal closure")
            client._is_running = False
            return
        await _successful_start(client)

    MockClient.start_hook = close_then_success
    try:
        tester = ReconnectionStatusExtensionTester()
        tester.set_test_mode_single(
            "tencent_asr_python", json.dumps(_property_json())
        )
        err = tester.run()
        assert (
            err is None
        ), f"test_connection_status_reconnect_on_close err: {err}"
        _assert_reconnection_sequence(tester.status_events)
    finally:
        MockClient.start_hook = None


class ParseErrorNoReconnectTester(ConnectionStatusExtensionTester):
    def __init__(self):
        super().__init__(timeout_seconds=5.0)
        self._post_connect_check_scheduled = False

    async def _verify_no_reconnect(self, ten_env_tester: AsyncTenEnvTester):
        await asyncio.sleep(2.0)
        currents = [event["current"] for event in self.status_events]
        if currents.count("connecting") > 1:
            ten_env_tester.stop_test(
                TenError.create(
                    error_code=TenErrorCode.ErrorCodeGeneric,
                    error_message=(
                        "9999 parse error should not reconnect; "
                        f"got status sequence: {currents}"
                    ),
                )
            )
            return
        if self.test_timeout_task:
            self.test_timeout_task.cancel()
        ten_env_tester.stop_test()

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        if data.get_name() != "connection_status_changed":
            return

        payload = json.loads(data.get_property_to_json()[0])
        self.status_events.append(payload)
        ten_env_tester.log_info(
            f"connection_status_changed: {payload['last']} -> {payload['current']}"
        )

        if (
            payload["current"] == "connected"
            and not self._post_connect_check_scheduled
        ):
            self._post_connect_check_scheduled = True
            asyncio.create_task(self._verify_no_reconnect(ten_env_tester))


def test_parse_error_does_not_reconnect(patch_tencent_asr_client):
    start_calls = {"count": 0}

    async def connect_then_parse_error(client: MockClient) -> None:
        start_calls["count"] += 1
        await _successful_start(client)
        await asyncio.sleep(0.3)
        await client.listener.on_asr_error(
            ResponseData[str](
                code=9999,
                message="error",
                voice_id="test-voice",
                result="invalid json",
            ),
            ValueError("invalid json"),
        )
        await asyncio.sleep(1.5)

    MockClient.start_hook = connect_then_parse_error
    try:
        tester = ParseErrorNoReconnectTester()
        tester.set_test_mode_single(
            "tencent_asr_python", json.dumps(_property_json())
        )
        err = tester.run()
        assert err is None, f"test_parse_error_does_not_reconnect err: {err}"
        currents = [event["current"] for event in tester.status_events]
        assert currents.count("connecting") == 1
        assert start_calls["count"] == 1
    finally:
        MockClient.start_hook = None
