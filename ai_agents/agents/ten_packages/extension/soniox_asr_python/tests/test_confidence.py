"""Unit tests for confidence score calculation."""

import pytest
from ..extension import SonioxASRExtension
from ..websocket import SonioxTranscriptToken


class TestConfidenceCalculation:
    """Test the _calculate_average_confidence method."""

    def setup_method(self):
        """Create extension instance for testing."""
        self.extension = SonioxASRExtension("test_extension")

    def test_all_tokens_have_confidence(self):
        """Test averaging when all tokens have confidence values."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=0.9),
            SonioxTranscriptToken("world", 500, 1000, True, confidence=0.8),
            SonioxTranscriptToken("test", 1000, 1500, True, confidence=0.95),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        expected = (0.9 + 0.8 + 0.95) / 3
        assert result is not None
        assert abs(result - expected) < 0.0001  # Use float comparison

    def test_some_tokens_missing_confidence(self):
        """Test averaging when some tokens have None confidence."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=0.9),
            SonioxTranscriptToken("world", 500, 1000, True, confidence=None),
            SonioxTranscriptToken("test", 1000, 1500, True, confidence=0.8),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        expected = (0.9 + 0.8) / 2
        assert result is not None
        assert abs(result - expected) < 0.0001

    def test_all_tokens_missing_confidence(self):
        """Test when all tokens have None confidence."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=None),
            SonioxTranscriptToken("world", 500, 1000, True, confidence=None),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        assert result is None

    def test_empty_token_list(self):
        """Test with empty token list."""
        tokens = []
        result = self.extension._calculate_average_confidence(tokens)
        assert result is None

    def test_single_token_with_confidence(self):
        """Test with a single token that has confidence."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=0.85),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        assert result == 0.85

    def test_single_token_without_confidence(self):
        """Test with a single token without confidence."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=None),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        assert result is None

    def test_confidence_range_boundaries(self):
        """Test with boundary values (0.0 and 1.0)."""
        tokens = [
            SonioxTranscriptToken("hello", 0, 500, True, confidence=0.0),
            SonioxTranscriptToken("world", 500, 1000, True, confidence=1.0),
        ]
        result = self.extension._calculate_average_confidence(tokens)
        assert result == 0.5
