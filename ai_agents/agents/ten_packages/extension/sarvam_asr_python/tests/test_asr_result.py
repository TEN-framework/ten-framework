import asyncio
import threading
from types import SimpleNamespace
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)
import json

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_sarvam_ws  # noqa: F401


class SarvamAsrExtensionTester(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.sender_task: asyncio.Task[None] | None = None
        self.stopped = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            if not chunk:
                break
            audio_frame = AudioFrame.create("pcm_frame")
            metadata = {"session_id": "123"}
            audio_frame.set_property_from_json("metadata", json.dumps(metadata))
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        self.sender_task = asyncio.create_task(
            self.audio_sender(ten_env_tester)
        )

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            err = TenError.create(
                error_code=TenErrorCode.ErrorCodeGeneric,
                error_message=error_message,
            )
            ten_env_tester.stop_test(err)

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        print(f"tester on_data, data_name: {data_name}")
        if data_name == "asr_result":
            # Check the data structure.

            data_json, _ = data.get_property_to_json()
            data_dict = json.loads(data_json)

            ten_env_tester.log_info(f"tester on_data, data_dict: {data_dict}")

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "id" in data_dict,
                f"id is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "text" in data_dict,
                f"text is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "final" in data_dict,
                f"final is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "start_ms" in data_dict,
                f"start_ms is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "duration_ms" in data_dict,
                f"duration_ms is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "language" in data_dict,
                f"language is not in data_dict: {data_dict}",
            )

            self.stop_test_if_checking_failed(
                ten_env_tester,
                "metadata" in data_dict,
                f"metadata is not in data_dict: {data_dict}",
            )

            session_id = data_dict.get("metadata", {}).get("session_id", "")
            self.stop_test_if_checking_failed(
                ten_env_tester,
                session_id == "123",
                f"session_id is not 123: {session_id}",
            )
            print(f"tester on_data, data_dict: {data_dict}")
            if data_dict["final"] == True:
                ten_env_tester.stop_test()

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_asr_result(patch_sarvam_ws):
    def trigger_transcript_messages():
        """Add WebSocket messages to simulate Sarvam API responses."""
        # Send a transcript message in Sarvam's format
        # The extension expects: {"type": "data", "data": {"transcript": "...", ...}}
        transcript_message = {
            "type": "data",
            "data": {
                "transcript": "hello world",
                "language_code": "en",
                "request_id": "test-request-123",
                "metrics": {
                    "audio_duration": 2.0,
                    "processing_latency": 0.1,
                },
            },
        }

        print("KEYPOINT adding transcript message to WebSocket")
        msg = patch_sarvam_ws.MockWebSocketMessage(
            msg_type=patch_sarvam_ws.WSMsgType.TEXT,
            data=json.dumps(transcript_message),
        )
        patch_sarvam_ws.add_message(msg)

    # Schedule message to be added after connection is established
    def delayed_message_sender():
        import time

        time.sleep(2)  # Wait for connection
        trigger_transcript_messages()

    # Start the message sender in a separate thread
    sender_thread = threading.Thread(target=delayed_message_sender, daemon=True)
    sender_thread.start()

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "sample_rate": 16000,
        }
    }

    tester = SarvamAsrExtensionTester()
    tester.set_test_mode_single("sarvam_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_asr_result err: {err}"
