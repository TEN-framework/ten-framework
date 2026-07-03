import asyncio
import json
import os
import tempfile
import uuid
from pathlib import Path
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

# Enable local mock to patch GoogleASRClient behavior
from .mock import patch_google_asr_client  # noqa: F401


SESSION_ID = "dump_test_session_123"


class GoogleAsrDumpTester(AsyncExtensionTester):
    """Tester that sends audio frames and triggers finalize, used to validate
    audio dump file is generated and matches sent content.
    """

    def __init__(self) -> None:
        super().__init__()
        self.sender_task: Union[asyncio.Task, None] = None
        self.sent_bytes = bytearray()

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
        finalize_id = f"finalize_{SESSION_ID}_{uuid.uuid4()}"
        payload = {
            "finalize_id": finalize_id,
            "metadata": {"session_id": SESSION_ID},
        }
        d = Data.create("asr_finalize")
        d.set_property_from_json(None, json.dumps(payload))
        await ten_env.send_data(d)

    async def audio_sender(self, ten_env: AsyncTenEnvTester) -> None:
        # Send a fixed number of frames
        for _ in range(30):
            chunk = b"\x00\x01" * 160
            self.sent_bytes.extend(chunk)
            frame = self._create_audio_frame(chunk, SESSION_ID)
            await ten_env.send_audio_frame(frame)
            await asyncio.sleep(0.01)

        # Finalize after sending audio
        await asyncio.sleep(0.2)
        await self._send_finalize(ten_env)

        # Give some time for finalize flow, then stop
        await asyncio.sleep(0.5)
        ten_env.stop_test()

    def _stop_with_error(
        self, ten_env: AsyncTenEnvTester, message: str
    ) -> None:
        err = TenError.create(
            error_code=TenErrorCode.ErrorCodeGeneric,
            error_message=message,
        )
        ten_env.stop_test(err)

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        # Run sequentially to keep timeline deterministic
        await self.audio_sender(ten_env_tester)

    @override
    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        if self.sender_task:
            _ = self.sender_task.cancel()
            try:
                await self.sender_task
            except asyncio.CancelledError:
                pass


def test_dump(patch_google_asr_client):  # noqa: F811
    # Prepare dump directory
    temp_dir = Path(tempfile.gettempdir()) / f"ten_dump_{uuid.uuid4()}"
    temp_dir.mkdir(parents=True, exist_ok=True)

    # Enable dump in property
    property_json = {
        "dump": True,
        "dump_path": str(temp_dir),
        "params": {
            "project_id": "fake-project-id",
            "language": "en-US",
            "model": "long",
        },
    }

    tester = GoogleAsrDumpTester()
    tester.set_test_mode_single("google_asr_python", json.dumps(property_json))
    err = tester.run()
    assert (
        err is None
    ), f"test_dump err: {err.error_message() if hasattr(err, 'error_message') else err}"

    # The extension writes to dump_path/google_asr_in.pcm
    dump_file = temp_dir / "google_asr_in.pcm"
    assert dump_file.exists(), f"Dump file not found: {dump_file}"
    file_bytes = dump_file.read_bytes()
    assert len(file_bytes) > 0, "Dump file is empty"
    # Verify content matches the concatenated bytes sent
    assert file_bytes == bytes(
        tester.sent_bytes
    ), f"Dump content mismatch: expected {len(tester.sent_bytes)} bytes, got {len(file_bytes)}"

    # Cleanup
    try:
        import shutil

        shutil.rmtree(temp_dir)
    except Exception:
        pass
