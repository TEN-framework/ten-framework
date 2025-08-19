import asyncio
import json
import time
from typing import Any, Union

from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)

# Enable local mock to patch GoogleASRClient behavior
from .mock import patch_google_asr_client  # noqa: F401


SESSION_ID = "finalize_test_session_123"


class GoogleAsrFinalizeTester(AsyncExtensionTester):
    """Tester that sends a few audio frames and an asr_finalize signal,
    then validates that asr_finalize_end is received and a final asr_result exists.
    """

    def __init__(self) -> None:
        super().__init__()
        self.sender_task: Union[asyncio.Task, None] = None
        self.finalize_id: str | None = None
        self.finalize_end_received: bool = False
        self.final_received: bool = False

    def _create_audio_frame(self, data: bytes, session_id: str) -> AudioFrame:
        audio_frame = AudioFrame.create("pcm_frame")
        metadata = {"session_id": session_id}
        audio_frame.set_property_from_json("metadata", json.dumps(metadata))
        audio_frame.alloc_buf(len(data))
        buf = audio_frame.lock_buf()
        buf[:] = data
        audio_frame.unlock_buf(buf)
        return audio_frame

    async def _send_finalize(self, ten_env: AsyncTenEnvTester) -> None:
        self.finalize_id = f"finalize_{SESSION_ID}_{int(time.time())}"
        payload = {
            "finalize_id": self.finalize_id,
            "metadata": {"session_id": SESSION_ID},
        }
        d = Data.create("asr_finalize")
        d.set_property_from_json(None, json.dumps(payload))
        await ten_env.send_data(d)

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        # Send several tiny audio chunks with session_id
        for _ in range(5):
            chunk = b"\x00\x01" * 160
            frame = self._create_audio_frame(chunk, SESSION_ID)
            await ten_env.send_audio_frame(frame)
            await asyncio.sleep(0.05)

        # Send finalize signal after audio
        await asyncio.sleep(0.2)
        await self._send_finalize(ten_env)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

    def _stop_with_error(
        self, ten_env: AsyncTenEnvTester, message: str
    ) -> None:
        err = TenError.create(
            error_code=TenErrorCode.ErrorCodeGeneric,
            error_message=message,
        )
        ten_env.stop_test(err)

    def _validate_required_fields(
        self, ten_env: AsyncTenEnvTester, data_json: dict[str, Any]
    ) -> bool:
        required = [
            "id",
            "text",
            "final",
            "start_ms",
            "duration_ms",
            "language",
        ]
        missing = [k for k in required if k not in data_json]
        if missing:
            self._stop_with_error(
                ten_env, f"Missing fields in asr_result: {missing}"
            )
            return False
        return True

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        if name == "asr_finalize_end":
            json_str, _ = data.get_property_to_json(None)
            obj: dict[str, Any] = json.loads(json_str)
            recv_finalize_id = obj.get("finalize_id")
            metadata = obj.get("metadata") or {}
            recv_session_id = metadata.get("session_id")

            if self.finalize_id is None:
                self._stop_with_error(
                    ten_env, "No finalize_id stored for comparison"
                )
                return
            if recv_finalize_id != self.finalize_id:
                self._stop_with_error(
                    ten_env,
                    f"finalize_id mismatch: expected {self.finalize_id}, got {recv_finalize_id}",
                )
                return
            if recv_session_id != SESSION_ID:
                self._stop_with_error(
                    ten_env,
                    f"session_id mismatch: expected {SESSION_ID}, got {recv_session_id}",
                )
                return

            self.finalize_end_received = True
            if self.final_received:
                ten_env.stop_test()
            return

        if name == "asr_result":
            json_str, _ = data.get_property_to_json(None)
            obj: dict[str, Any] = json.loads(json_str)

            if not self._validate_required_fields(ten_env, obj):
                return

            if obj.get("final") is True:
                self.final_received = True
                if self.finalize_end_received:
                    ten_env.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_asr_finalize(patch_google_asr_client):  # noqa: F811
    property_json = {
        "params": {
            "project_id": "fake-project-id",
            "language": "en-US",
            "model": "long",
        }
    }

    tester = GoogleAsrFinalizeTester()
    tester.set_test_mode_single("google_asr_python", json.dumps(property_json))
    err = tester.run()
    assert (
        err is None
    ), f"test_asr_finalize err: {err.error_message() if hasattr(err, 'error_message') else err}"
