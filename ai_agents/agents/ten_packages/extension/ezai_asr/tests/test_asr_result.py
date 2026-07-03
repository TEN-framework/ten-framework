import asyncio
import threading
from types import SimpleNamespace
from typing import Union
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

from .mock import patch_ezai_ws  # noqa: F401


class EzaiAsrExtensionTester(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.sender_task: Union[asyncio.Task, None] = None
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


def test_asr_result(patch_ezai_ws):
    async def trigger_transcript_events():
        async def trigger_open_event():
            print("KEYPOINT trigger_open_event")
            await patch_ezai_ws.event_handlers["open"]({}, SimpleNamespace())
            await asyncio.sleep(1)
            await trigger_interim_transcript()
            await asyncio.sleep(2)
            await trigger_final_transcript()

        async def trigger_interim_transcript():
            print("KEYPOINT trigger_interim_transcript")
            result = SimpleNamespace(
                text="hello",
                type="realtime",
                start=0.0,
                duration=1.0,
                language="en-US",
                final=False,
            )
            await patch_ezai_ws.event_handlers["transcript"](
                {}, result.__dict__
            )

        async def trigger_final_transcript():
            print("KEYPOINT trigger_final_transcript")
            result = SimpleNamespace(
                text="hello world",
                type="fullSentence",
                start=0.0,
                duration=2.0,
                language="en-US",
                final=True,
            )
            await patch_ezai_ws.event_handlers["transcript"](
                {}, result.__dict__
            )

        await asyncio.sleep(5)
        await trigger_open_event()

    async def mock_start():
        await trigger_transcript_events()
        return True

    patch_ezai_ws.client_instance.start.side_effect = mock_start

    property_json = {
        "params": {
            "token": "fake_token",
            "sample_rate": 16000,
        }
    }

    tester = EzaiAsrExtensionTester()
    tester.set_test_mode_single("ezai_asr", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_asr_result err: {err}"
