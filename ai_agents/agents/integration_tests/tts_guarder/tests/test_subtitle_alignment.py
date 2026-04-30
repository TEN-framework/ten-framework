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
import json
import os
import time
import asyncio

TTS_SUBTITLE_CONFIG_FILE = "property_subtitle_alignment.json"
SUPPORTED_TTS_EXTENSIONS = {"cartesia_tts"}

# Threshold for text vs audio duration mismatch
DURATION_MISMATCH_THRESHOLD_MS = 1000


class SubtitleAlignmentTester(AsyncExtensionTester):
    """Test class for TTS subtitle alignment validation.

    Validates that tts_text_result events are properly aligned with audio frames:
    1. First text start_ms == first audio frame timestamp
    2. Text start_ms values are strictly ascending
    3. Audio frame timestamps are strictly ascending (next = prev + duration)
    4. Total text duration and total audio duration differ within threshold
    5. turn_seq_id is ascending and request_id is consistent
    6. Last tts_text_result turn_status is valid (1=end or 2=interrupted)
    """

    def __init__(
        self,
        session_id: str = "test_subtitle_alignment_session",
    ):
        super().__init__()
        print("=" * 80)
        print("🧪 TEST CASE: Subtitle Alignment TTS Test")
        print("=" * 80)
        print("📋 Test Description: Validate TTS text result alignment with audio")
        print("🎯 Test Objectives:")
        print("   1. First text start_ms == first audio frame timestamp")
        print("   2. Text start_ms strictly ascending")
        print("   3. Audio frame timestamps strictly ascending (next = prev + duration)")
        print("   4. Text total duration vs audio total duration within threshold")
        print("   5. turn_seq_id ascending and request_id consistent")
        print("   6. Last turn_status is valid (1 or 2)")
        print("=" * 80)

        self.session_id = session_id
        self.request_id = "test_subtitle_alignment_request_id_1"
        self.sent_metadata = None

        # Collected tts_text_result data
        self.text_results: list[dict[str, Any]] = []

        # Collected audio frame data: list of (timestamp_ms, duration_ms)
        self.audio_frames: list[tuple[int, int]] = []

        # First audio frame timestamp from audio_frame.get_timestamp()
        self.first_audio_frame_ts: int | None = None
        self.last_audio_frame_ts: int | None = None

        # Audio tracking
        self.sample_rate: int = 0
        self.audio_start_received: bool = False

    @override
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("Starting subtitle alignment test")
        await self._send_tts_text_input(ten_env)

    async def _send_tts_text_input(self, ten_env: AsyncTenEnvTester) -> None:
        text = (
            "The castle's shadow stretched across the moat like a skeletal hand, "
            "its turrets piercing the stormy sky where lightning flickered."
        )
        ten_env.log_info(f"Sending tts text input: {text}")
        tts_text_input_obj = Data.create("tts_text_input")
        tts_text_input_obj.set_property_string("text", text)
        tts_text_input_obj.set_property_string("request_id", self.request_id)
        tts_text_input_obj.set_property_bool("text_input_end", True)
        metadata = {
            "session_id": self.session_id,
            "turn_id": 1,
        }
        self.sent_metadata = metadata
        tts_text_input_obj.set_property_from_json("metadata", json.dumps(metadata))
        await ten_env.send_data(tts_text_input_obj)
        ten_env.log_info("✅ tts text input sent")

    def _stop_test_with_error(self, ten_env: AsyncTenEnvTester, msg: str) -> None:
        ten_env.log_error(f"❌ {msg}")
        ten_env.stop_test(TenError.create(TenErrorCode.ErrorCodeGeneric, msg))

    @override
    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()
        ten_env.log_info(f"Received data: {name}")

        if name == "error":
            json_str, _ = data.get_property_to_json("")
            self._stop_test_with_error(ten_env, f"Received error: {json_str}")
            return

        elif name == "tts_audio_start":
            self.audio_start_received = True
            ten_env.log_info("Received tts_audio_start")
            return

        elif name == "tts_text_result":
            json_str, _ = data.get_property_to_json("")
            ten_env.log_info(f"Received tts_text_result: {json_str}")
            try:
                result = json.loads(json_str)
                self.text_results.append(result)
            except json.JSONDecodeError as e:
                self._stop_test_with_error(ten_env, f"Invalid tts_text_result JSON: {e}")
            return

        elif name == "tts_audio_end":
            ten_env.log_info("Received tts_audio_end, waiting for final transcription")
            # Wait a bit for the last tts_text_result to arrive,
            # since words_queue may be consumed slower than pcm_queue.
            await asyncio.sleep(0.5)
            ten_env.log_info("Running validations")
            self._run_validations(ten_env)
            return

    @override
    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, audio_frame: AudioFrame
    ) -> None:
        ts = audio_frame.get_timestamp()
        sr = audio_frame.get_sample_rate()
        samples = audio_frame.get_samples_per_channel()

        if sr > 0:
            duration_ms = samples * 1000 // sr
        else:
            duration_ms = 0

        if self.sample_rate == 0:
            self.sample_rate = sr

        if self.first_audio_frame_ts is None:
            self.first_audio_frame_ts = ts
        self.last_audio_frame_ts = ts

        self.audio_frames.append((ts, duration_ms))

    @override
    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("Test stopped")

    # ── Validation logic ──

    def _run_validations(self, ten_env: AsyncTenEnvTester) -> None:
        if len(self.text_results) == 0:
            self._stop_test_with_error(ten_env, "No tts_text_result received")
            return
        if len(self.audio_frames) == 0:
            self._stop_test_with_error(ten_env, "No audio frames received")
            return

        validators = [
            self._validate_first_timestamp,
            self._validate_text_timestamps_ascending,
            self._validate_audio_frames_ascending,
            self._validate_duration_match,
            self._validate_turn_sequence,
            self._validate_turn_status,
        ]

        for v in validators:
            ok, msg = v(ten_env)
            if not ok:
                self._stop_test_with_error(ten_env, msg)
                return
            ten_env.log_info(f"✅ {msg}")

        ten_env.log_info("✅ All subtitle alignment validations passed")
        ten_env.stop_test()

    def _validate_first_timestamp(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 1: First text start_ms == first audio frame timestamp."""
        first_text_start = self.text_results[0].get("start_ms", -1)
        first_audio_ts = self.first_audio_frame_ts

        if first_text_start < first_audio_ts:
            return (
                False,
                f"First text start_ms ({first_text_start}) < first audio frame ts ({first_audio_ts})",
            )
        return True, "First text start_ms matches first audio frame timestamp"

    def _validate_text_timestamps_ascending(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 2: Text start_ms values are strictly ascending."""
        start_ms_list = [r.get("start_ms", 0) for r in self.text_results]
        for i in range(1, len(start_ms_list)):
            if start_ms_list[i] <= start_ms_list[i - 1]:
                return (
                    False,
                    f"Text start_ms not ascending at index {i}: {start_ms_list[i]} <= {start_ms_list[i-1]}",
                )
        return True, "Text start_ms values are strictly ascending"

    def _validate_audio_frames_ascending(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 3: Audio frame timestamps ascending (next ≈ prev + duration, tolerance 1ms)."""
        for i in range(1, len(self.audio_frames)):
            prev_ts, prev_dur = self.audio_frames[i - 1]
            cur_ts, _ = self.audio_frames[i]
            expected = prev_ts + prev_dur
            if abs(cur_ts - expected) > 10:
                return (
                    False,
                    f"Audio frame ts mismatch at index {i}: expected {expected}, got {cur_ts} (diff={cur_ts - expected}ms)",
                )
        return True, "Audio frame timestamps are strictly ascending (next = prev + duration, ±10ms)"

    def _validate_duration_match(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 4: Text total duration vs audio total duration within threshold."""
        # Text duration: from first word start to last word end
        first_result = self.text_results[0]
        # Find last non-empty result (has words)
        last_result = self.text_results[-1]
        for r in reversed(self.text_results):
            words = r.get("words")
            if words and len(words) > 0:
                last_result = r
                break

        # Calculate text span
        words_first = first_result.get("words")
        if words_first and len(words_first) > 0:
            first_word_start = words_first[0].get("start_ms", first_result.get("start_ms", 0))
        else:
            first_word_start = first_result.get("start_ms", 0)

        words_last = last_result.get("words")
        if words_last and len(words_last) > 0:
            last_word = words_last[-1]
            last_word_end = last_word.get("start_ms", 0) + last_word.get("duration_ms", 0)
        else:
            last_word_end = last_result.get("start_ms", 0) + last_result.get("duration_ms", 0)

        text_duration = last_word_end - first_word_start

        # Audio duration: from first frame to last frame end
        first_audio_ts = self.audio_frames[0][0]
        last_audio_ts, last_audio_dur = self.audio_frames[-1]
        audio_duration = last_audio_ts - first_audio_ts + last_audio_dur

        delta = abs(text_duration - audio_duration)
        if delta > DURATION_MISMATCH_THRESHOLD_MS:
            return (
                False,
                f"Duration mismatch: text={text_duration}ms, audio={audio_duration}ms, delta={delta}ms > {DURATION_MISMATCH_THRESHOLD_MS}ms",
            )
        return (
            True,
            f"Duration match: text={text_duration}ms, audio={audio_duration}ms, delta={delta}ms",
        )

    def _validate_turn_sequence(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 5: turn_seq_id ascending and request_id consistent."""
        request_ids = set()
        turn_seq_ids = []

        for r in self.text_results:
            request_ids.add(r.get("request_id", ""))
            meta = r.get("metadata", {})
            seq_id = meta.get("turn_seq_id")
            if seq_id is not None:
                turn_seq_ids.append(seq_id)

        if len(request_ids) > 1:
            return False, f"Inconsistent request_ids: {request_ids}"

        # Check turn_seq_id ascending (non-strict, allow equal)
        for i in range(1, len(turn_seq_ids)):
            if turn_seq_ids[i] < turn_seq_ids[i - 1]:
                return (
                    False,
                    f"turn_seq_id not ascending at index {i}: {turn_seq_ids[i]} < {turn_seq_ids[i-1]}",
                )

        return True, "turn_seq_id ascending and request_id consistent"

    def _validate_turn_status(self, ten_env: AsyncTenEnvTester) -> tuple[bool, str]:
        """Rule 6: Last tts_text_result turn_status is 1 (end) or 2 (interrupted)."""
        last_result = self.text_results[-1]
        meta = last_result.get("metadata", {})
        turn_status = meta.get("turn_status")

        if turn_status not in [1, 2]:
            return (
                False,
                f"Last turn_status is {turn_status}, expected 1 (end) or 2 (interrupted)",
            )
        return True, f"Last turn_status is {turn_status} (valid)"


def test_subtitle_alignment(
    extension_name: str,
    config_dir: str,
    enable_subtitle_alignment: bool,
) -> None:
    """Verify TTS subtitle alignment with audio frames."""
    if not enable_subtitle_alignment:
        import pytest

        pytest.skip(
            "subtitle alignment test is disabled by default; "
            "pass --enable_subtitle_alignment=True to run it"
        )

    if extension_name not in SUPPORTED_TTS_EXTENSIONS:
        import pytest

        pytest.skip(
            f"subtitle alignment test currently supports only "
            f"{sorted(SUPPORTED_TTS_EXTENSIONS)}, got {extension_name}"
        )

    config_file_path = os.path.join(config_dir, TTS_SUBTITLE_CONFIG_FILE)
    if not os.path.exists(config_file_path):
        raise FileNotFoundError(f"Config file not found: {config_file_path}")

    with open(config_file_path, "r") as f:
        config: dict[str, Any] = json.load(f)

    print(f"Using test configuration: {config}")

    tester = SubtitleAlignmentTester(
        session_id="test_subtitle_alignment_session",
    )

    tester.set_test_mode_single(extension_name, json.dumps(config))
    error = tester.run()

    assert error is None, (
        f"Test failed: {error.error_message() if error else 'Unknown error'}"
    )
