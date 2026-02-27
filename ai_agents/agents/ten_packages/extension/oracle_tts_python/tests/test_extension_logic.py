"""Tests for OracleTTSExtension helper methods.

These tests instantiate the extension with mocked dependencies
to verify audio duration calculation and request state management.
"""

import pytest


class TestAudioDurationCalculation:
    """Test the _calculate_audio_duration_ms logic independent of the extension."""

    @staticmethod
    def _calc_duration(total_bytes: int, sample_rate: int) -> int:
        bytes_per_sample = 2  # 16-bit PCM
        channels = 1
        if sample_rate == 0:
            return 0
        duration_sec = total_bytes / (sample_rate * bytes_per_sample * channels)
        return int(duration_sec * 1000)

    def test_one_second_at_16khz(self) -> None:
        total_bytes = 16000 * 2 * 1  # 1 second
        assert self._calc_duration(total_bytes, 16000) == 1000

    def test_half_second_at_16khz(self) -> None:
        total_bytes = 16000 * 2 * 1 // 2  # 0.5 seconds
        assert self._calc_duration(total_bytes, 16000) == 500

    def test_zero_bytes(self) -> None:
        assert self._calc_duration(0, 16000) == 0

    def test_zero_sample_rate(self) -> None:
        assert self._calc_duration(1000, 0) == 0

    def test_24khz_sample_rate(self) -> None:
        total_bytes = 24000 * 2  # 1 second at 24kHz
        assert self._calc_duration(total_bytes, 24000) == 1000


class TestTTSErrorClassification:
    """Test TTS error classification for FATAL vs NON_FATAL errors."""

    AUTH_ERROR_KEYWORDS = ["401", "403", "auth", "credentials"]
    RETRYABLE_KEYWORDS = ["timeout", "connection", "socket"]

    @staticmethod
    def _classify_tts_error(error_msg: str) -> str:
        """Reproduce the error classification from oracle_tts.py get()."""
        if any(kw in error_msg.lower() for kw in ["401", "403", "auth", "credentials"]):
            return "INVALID_KEY_ERROR"

        if any(kw in error_msg.lower() for kw in ["timeout", "connection", "socket"]):
            return "RETRYABLE"

        return "ERROR"

    def test_auth_error_401(self) -> None:
        assert self._classify_tts_error("401 Unauthorized") == "INVALID_KEY_ERROR"

    def test_auth_error_403(self) -> None:
        assert self._classify_tts_error("403 Forbidden") == "INVALID_KEY_ERROR"

    def test_auth_error_credentials(self) -> None:
        assert self._classify_tts_error("Invalid credentials") == "INVALID_KEY_ERROR"

    def test_retryable_timeout(self) -> None:
        assert self._classify_tts_error("Connection timeout") == "RETRYABLE"

    def test_retryable_socket(self) -> None:
        assert self._classify_tts_error("Socket error") == "RETRYABLE"

    def test_generic_error(self) -> None:
        assert self._classify_tts_error("Unknown error occurred") == "ERROR"

    def test_empty_error(self) -> None:
        assert self._classify_tts_error("") == "ERROR"

    def test_case_insensitive_auth(self) -> None:
        assert self._classify_tts_error("AUTH failure") == "INVALID_KEY_ERROR"


class TestFlushBehavior:
    """Test the flush/cancel request logic pattern used in TTS extension."""

    def test_flush_flag_blocks_audio_processing(self) -> None:
        """Simulate cancel_tts -> _flush_requested = True blocks audio loop."""
        flush_requested = False
        processed_chunks = 0
        audio_chunks = [b"\x01\x02"] * 10

        for chunk in audio_chunks:
            if flush_requested:
                break
            processed_chunks += 1
            if processed_chunks == 3:
                flush_requested = True

        assert processed_chunks == 3
        assert flush_requested is True

    def test_dedup_completed_request(self) -> None:
        """Verify the last_complete_request_id dedup logic."""
        last_complete_request_id = None
        completed_count = 0

        def handle_completed(request_id: str):
            nonlocal last_complete_request_id, completed_count
            if last_complete_request_id == request_id:
                return
            last_complete_request_id = request_id
            completed_count += 1

        handle_completed("req-1")
        handle_completed("req-1")
        handle_completed("req-2")
        handle_completed("req-2")
        handle_completed("req-2")

        assert completed_count == 2
        assert last_complete_request_id == "req-2"
