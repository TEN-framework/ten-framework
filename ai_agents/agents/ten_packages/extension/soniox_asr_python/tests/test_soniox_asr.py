import asyncio
import json
from types import SimpleNamespace

from ten_packages.extension.soniox_asr_python.websocket import (
    SonioxFinToken, SonioxTranscriptToken)
from ten_runtime import (AsyncExtensionTester, AsyncTenEnvTester, AudioFrame,
                         Data, TenError, TenErrorCode)
from typing_extensions import override

from .mock import patch_soniox_ws  # noqa: F401


class SonioxAsrExtensionTester(AsyncExtensionTester):

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
        ten_env_tester.log_info(f"tester on_data, data: {data}")
        data_name = data.get_name()
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
                "words" in data_dict,
                f"words is not in data_dict: {data_dict}",
            )

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


def test_asr_result(patch_soniox_ws):
    async def fake_connect():
        # Simulate connection opening
        await patch_soniox_ws.websocket_client.trigger_open()

        # Wait a bit, then send transcript events
        await asyncio.sleep(0.1)

        # Mock transcript tokens

        # First non-final token
        token1 = SonioxTranscriptToken(
            text="hello", start_ms=0, end_ms=500, is_final=False, language="en"
        )

        # Send first transcript
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token1], 0, 0
        )

        await asyncio.sleep(0.1)

        # Final token
        token2 = SonioxTranscriptToken(
            text="hello world",
            start_ms=0,
            end_ms=1000,
            is_final=True,
            language="en",
        )

        # Fin token
        fin_token = SonioxFinToken("<fin>", True)

        # Send final transcript with fin token
        await patch_soniox_ws.websocket_client.trigger_transcript(
            [token2, fin_token], 0, 0
        )

    async def fake_send_audio(_audio_data):
        await asyncio.sleep(0)

    async def fake_finalize():
        await asyncio.sleep(0)

    async def fake_stop():
        await asyncio.sleep(0)

    # Inject into websocket client
    patch_soniox_ws.websocket_client.connect.side_effect = fake_connect
    patch_soniox_ws.websocket_client.send_audio.side_effect = fake_send_audio
    patch_soniox_ws.websocket_client.finalize.side_effect = fake_finalize
    patch_soniox_ws.websocket_client.stop.side_effect = fake_stop

    property_json = {
        "params": {
            "api_key": "fake_api_key",
            "url": "wss://fake.soniox.com/transcribe-websocket",
            "sample_rate": 16000,
            "dump": False,
            "dump_path": ".",
        }
    }

    tester = SonioxAsrExtensionTester()
    tester.set_test_mode_single("soniox_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, f"test_asr_result err: {err}"
