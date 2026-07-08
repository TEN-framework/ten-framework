import asyncio
import json

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)
from typing_extensions import override


class ConnectionStatusExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False
        self.status_events: list[dict] = []
        self.test_timeout_task: asyncio.Task[None] | None = None

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
        await asyncio.sleep(3.0)
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

        assert payload["vendor_info"]["vendor"] == "soniox"
        assert payload["metadata"]["vendor_metadata"]["url"] == (
            "wss://stt-rt.soniox.com/transcribe-websocket"
        )

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


def test_connection_status_changed(patch_soniox_ws):
    import time
    from .conftest import create_fake_websocket_mocks, inject_websocket_mocks

    async def custom_connect():
        connection_start_timestamp = int(time.time() * 1000)
        await asyncio.sleep(0.1)
        await patch_soniox_ws.websocket_client.trigger_open(
            connection_start_timestamp
        )
        await asyncio.sleep(0.1)

    mocks = create_fake_websocket_mocks(
        patch_soniox_ws, on_connect=custom_connect
    )
    inject_websocket_mocks(patch_soniox_ws, mocks)

    property_json = {"params": {"api_key": "fake_key"}}

    tester = ConnectionStatusExtensionTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_connection_status_changed err: {err}"
    assert any(
        event["current"] == "connecting" for event in tester.status_events
    )
    assert any(
        event["current"] == "connected" for event in tester.status_events
    )
