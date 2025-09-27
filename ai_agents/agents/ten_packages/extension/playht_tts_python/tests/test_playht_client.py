import unittest
from pydantic import ValidationError

from ..playht_tts import PlayHTParams
from pyht.client import TTSOptions, Language, Format


class TestPlayHTParams(unittest.TestCase):
    """Test PlayHTParams class"""

    def test_playht_params_creation(self):
        """Test creating PlayHTParams with valid data"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            voice_engine="PlayDialog",
            protocol="ws",
            format=Format.FORMAT_PCM,
            sample_rate=16000,
            voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        )

        self.assertEqual(params.api_key, "test_api_key")
        self.assertEqual(params.user_id, "test_user_id")
        self.assertEqual(params.voice_engine, "PlayDialog")
        self.assertEqual(params.protocol, "ws")
        self.assertEqual(params.format, Format.FORMAT_PCM)
        self.assertEqual(params.sample_rate, 16000)

    def test_playht_params_defaults(self):
        """Test PlayHTParams with default values"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        )

        self.assertEqual(params.protocol, "ws")
        self.assertEqual(params.format, Format.FORMAT_PCM)
        self.assertEqual(params.sample_rate, 16000)
        self.assertIsNone(params.voice_engine)
        self.assertIsNone(params.language)

    def test_playht_params_format_validation(self):
        """Test format validation"""
        # Test with string format
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            format="FORMAT_PCM",
            voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        )
        self.assertEqual(params.format, Format.FORMAT_PCM)

        # Test with invalid format
        with self.assertRaises(ValidationError):
            PlayHTParams(
                api_key="test_api_key",
                user_id="test_user_id",
                format="INVALID_FORMAT",
                voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
            )

    def test_playht_params_to_tts_options(self):
        """Test conversion to TTSOptions"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            voice_engine="PlayDialog",
            protocol="ws",
            format=Format.FORMAT_PCM,
            sample_rate=16000,
            language=Language.ENGLISH,
            voice="s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        )

        tts_options = params.to_tts_options()
        self.assertIsInstance(tts_options, TTSOptions)
        self.assertEqual(tts_options.format, Format.FORMAT_PCM)
        self.assertEqual(tts_options.sample_rate, 16000)
        self.assertEqual(tts_options.language, Language.ENGLISH)
