#!/usr/bin/env python3
#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from typing import Any
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)
import asyncio
import json
import os


AUDIO_CHUNK_SIZE = 320
FRAME_INTERVAL_MS = 10
RESULT_WAIT_TIMEOUT_SECS = 15
FINALIZE_WAIT_TIMEOUT_SECS = 10

CONFIG_FILE = "property_en.json"
SESSION_ID = "test_same_session_finalize_reconnect_session_123"
EXPECTED_LANGUAGE = "en-US"


class SameSessionFinalizeReconnectTester(AsyncExtensionTester):
    """Validate that one session survives multiple finalize cycles."""

    def __init__(
        self,
        audio_file_path: str,
        session_id: str = SESSION_ID,
        expected_language: str = EXPECTED_LANGUAGE,
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Same Session Finalize Reconnect Test")
        print("=" * 80)
        print(
            "📋 Test Description: Validate that ASR keeps working after finalize in the same session"
        )
        print("🎯 Test Objectives:")
        print("   - Send audio in one session")
        print("   - Finalize and receive final result + finalize_end")
        print("   - Send audio again in the same session")
        print("   - Finalize again and require another non-empty final result")
        print("=" * 80)

        self.audio_file_path = audio_file_path
        self.session_id = session_id
        self.expected_language = expected_language

        self.current_cycle = 0
        self.current_finalize_id: str | None = None
        self.current_cycle_final_text: str | None = None

        self.result_event = asyncio.Event()
        self.finalize_end_event = asyncio.Event()
        self.cycle_results: list[dict[str, Any]] = []

    def _stop_test_with_error(
        self, ten_env: AsyncTenEnvTester, error_message: str
    ) -> None:
        ten_env.stop_test(
            TenError.create(TenErrorCode.ErrorCodeGeneric, error_message)
        )

    def _create_audio_frame(self, data: bytes, session_id: str) -> AudioFrame:
        audio_frame = AudioFrame.create("pcm_frame")
        metadata = {"session_id": session_id}
        audio_frame.set_property_from_json("metadata", json.dumps(metadata))
        audio_frame.alloc_buf(len(data))
        buf = audio_frame.lock_buf()
        buf[:] = data
        audio_frame.unlock_buf(buf)
        return audio_frame

    async def _send_audio_file(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info(
            f"Sending audio file for cycle {self.current_cycle}: {self.audio_file_path}"
        )
        with open(self.audio_file_path, "rb") as audio_file:
            while True:
                chunk = audio_file.read(AUDIO_CHUNK_SIZE)
                if not chunk:
                    break
                audio_frame = self._create_audio_frame(chunk, self.session_id)
                await ten_env.send_audio_frame(audio_frame)
                await asyncio.sleep(FRAME_INTERVAL_MS / 1000)

    async def _send_finalize_signal(self, ten_env: AsyncTenEnvTester) -> None:
        self.current_finalize_id = (
            f"finalize_{self.session_id}_{self.current_cycle}_"
            f"{int(asyncio.get_event_loop().time())}"
        )
        finalize_data = {
            "finalize_id": self.current_finalize_id,
            "metadata": {"session_id": self.session_id},
        }
        finalize_data_obj = Data.create("asr_finalize")
        finalize_data_obj.set_property_from_json(
            None, json.dumps(finalize_data)
        )
        await ten_env.send_data(finalize_data_obj)
        ten_env.log_info(
            f"✅ asr_finalize sent for cycle {self.current_cycle}: {self.current_finalize_id}"
        )

    async def _run_cycle(self, ten_env: AsyncTenEnvTester, cycle: int) -> None:
        self.current_cycle = cycle
        self.current_cycle_final_text = None
        self.current_finalize_id = None
        self.result_event.clear()
        self.finalize_end_event.clear()

        ten_env.log_info(f"=== Starting cycle {cycle} in session {self.session_id} ===")
        await self._send_audio_file(ten_env)
        await asyncio.sleep(1.5)
        await self._send_finalize_signal(ten_env)

        try:
            await asyncio.wait_for(
                self.result_event.wait(), timeout=RESULT_WAIT_TIMEOUT_SECS
            )
        except asyncio.TimeoutError:
            self._stop_test_with_error(
                ten_env,
                f"Timed out waiting for final ASR result in cycle {cycle}",
            )
            return

        try:
            await asyncio.wait_for(
                self.finalize_end_event.wait(),
                timeout=FINALIZE_WAIT_TIMEOUT_SECS,
            )
        except asyncio.TimeoutError:
            self._stop_test_with_error(
                ten_env,
                f"Timed out waiting for asr_finalize_end in cycle {cycle}",
            )
            return

    def _validate_required_fields(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        required_fields = [
            "id",
            "text",
            "final",
            "start_ms",
            "duration_ms",
            "language",
        ]
        missing_fields = [
            field for field in required_fields if field not in json_data
        ]
        if missing_fields:
            self._stop_test_with_error(
                ten_env, f"Missing required fields: {missing_fields}"
            )
            return False
        return True

    def _validate_final_result(
        self, ten_env: AsyncTenEnvTester, json_data: dict[str, Any]
    ) -> bool:
        language = json_data.get("language", "")
        if language != self.expected_language:
            self._stop_test_with_error(
                ten_env,
                f"Language mismatch, expected: {self.expected_language}, actual: {language}",
            )
            return False

        metadata = json_data.get("metadata")
        if (
            not isinstance(metadata, dict)
            or metadata.get("session_id") != self.session_id
        ):
            self._stop_test_with_error(
                ten_env,
                f"session_id mismatch, expected: {self.session_id}, actual: {metadata}",
            )
            return False

        text = str(json_data.get("text", ""))
        if not text.strip():
            self._stop_test_with_error(
                ten_env,
                f"Cycle {self.current_cycle} produced empty final transcription",
            )
            return False

        return True

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await self._run_cycle(ten_env, cycle=1)
        # Give single-utterance vendors a short window to reopen cleanly.
        await asyncio.sleep(1.0)
        await self._run_cycle(ten_env, cycle=2)

        if len(self.cycle_results) != 2:
            self._stop_test_with_error(
                ten_env,
                f"Expected 2 successful cycles, got {len(self.cycle_results)}",
            )
            return

        ten_env.log_info(
            "✅ Same-session finalize reconnect test passed with 2 non-empty final results"
        )
        ten_env.stop_test()

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()

        if name == "asr_finalize_end":
            json_str, _ = data.get_property_to_json(None)
            finalize_end_data: dict[str, Any] = json.loads(json_str)
            finalize_id = finalize_end_data.get("finalize_id")
            metadata = finalize_end_data.get("metadata")

            if finalize_id != self.current_finalize_id:
                self._stop_test_with_error(
                    ten_env,
                    f"Unexpected finalize_end id in cycle {self.current_cycle}: "
                    f"expected {self.current_finalize_id}, got {finalize_id}",
                )
                return

            if (
                not isinstance(metadata, dict)
                or metadata.get("session_id") != self.session_id
            ):
                self._stop_test_with_error(
                    ten_env,
                    f"Unexpected finalize_end metadata in cycle {self.current_cycle}: {metadata}",
                )
                return

            ten_env.log_info(
                f"✅ finalize_end received for cycle {self.current_cycle}: {finalize_id}"
            )
            self.finalize_end_event.set()
            return

        if name != "asr_result":
            ten_env.log_info(f"Received non-ASR data: {name}")
            return

        json_str, _ = data.get_property_to_json(None)
        json_data: dict[str, Any] = json.loads(json_str)
        if not self._validate_required_fields(ten_env, json_data):
            return

        is_final = bool(json_data.get("final", False))
        text = str(json_data.get("text", ""))
        ten_env.log_info(
            f"Received ASR result in cycle {self.current_cycle} - "
            f"final: {is_final}, text: {text!r}"
        )

        if not is_final:
            return
        if self.result_event.is_set():
            ten_env.log_info(
                f"Ignoring additional final result in cycle {self.current_cycle}"
            )
            return
        if not self._validate_final_result(ten_env, json_data):
            return

        self.current_cycle_final_text = text
        self.cycle_results.append(
            {
                "cycle": self.current_cycle,
                "id": json_data.get("id"),
                "text": text,
            }
        )
        ten_env.log_info(
            f"✅ Final ASR result received in cycle {self.current_cycle}: {text!r}"
        )
        self.result_event.set()

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("Test stopped")


def test_same_session_finalize_reconnect(
    extension_name: str, config_dir: str
) -> None:
    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data/16k_en_us.pcm"
    )
    config_file_path = os.path.join(config_dir, CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    print(f"Using test configuration: {config}")
    print(f"Audio file path: {audio_file_path}")
    print(
        f"Expected results: language='{EXPECTED_LANGUAGE}', session_id='{SESSION_ID}'"
    )

    tester = SameSessionFinalizeReconnectTester(
        audio_file_path=audio_file_path,
        session_id=SESSION_ID,
        expected_language=EXPECTED_LANGUAGE,
    )
    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    assert (
        error is None
    ), f"Test failed: {error.error_message() if error else 'Unknown error'}"
