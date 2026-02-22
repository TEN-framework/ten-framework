"""Tests for ASR error classification logic.

Verifies that fatal indicators (401, 403, AuthFail, InvalidParameter)
produce FATAL_ERROR, while other errors produce NON_FATAL_ERROR.
"""

import pytest


FATAL_INDICATORS = ["401", "403", "InvalidParameter", "AuthFail"]


def _classify_error(error_msg: str) -> str:
    """Reproduce the error classification logic from OracleASRExtension.on_error."""
    if any(ind in str(error_msg) for ind in FATAL_INDICATORS):
        return "FATAL_ERROR"
    return "NON_FATAL_ERROR"


class TestErrorClassification:
    @pytest.mark.parametrize("indicator", FATAL_INDICATORS)
    def test_fatal_indicator_detected(self, indicator: str) -> None:
        error_msg = f"OCI error: {indicator} - something went wrong"
        assert _classify_error(error_msg) == "FATAL_ERROR"

    def test_non_fatal_generic_error(self) -> None:
        assert _classify_error("Connection reset by peer") == "NON_FATAL_ERROR"

    def test_non_fatal_timeout(self) -> None:
        assert _classify_error("Connection timeout after 10 seconds") == "NON_FATAL_ERROR"

    def test_non_fatal_network_error(self) -> None:
        assert _classify_error("WebSocket connection closed") == "NON_FATAL_ERROR"

    def test_fatal_auth_in_longer_message(self) -> None:
        msg = "OCI SDK returned status 401 Unauthorized for region us-phoenix-1"
        assert _classify_error(msg) == "FATAL_ERROR"

    def test_fatal_403_forbidden(self) -> None:
        msg = "Access denied: 403 Forbidden"
        assert _classify_error(msg) == "FATAL_ERROR"

    def test_empty_error_message(self) -> None:
        assert _classify_error("") == "NON_FATAL_ERROR"

    def test_numeric_only(self) -> None:
        assert _classify_error("500 Internal Server Error") == "NON_FATAL_ERROR"

    def test_case_sensitive_authfail(self) -> None:
        """AuthFail is case-sensitive in the implementation."""
        assert _classify_error("authfail lowercase") == "NON_FATAL_ERROR"
        assert _classify_error("AuthFail uppercase") == "FATAL_ERROR"
