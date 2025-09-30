import unittest
import json
from pydantic import ValidationError

from ..config import PlayHTTTSConfig
from ..playht_tts import PlayHTParams
from pyht.client import Format, Language


class TestPlayHTTTSConfig(unittest.TestCase):
    """Test PlayHTTTSConfig class"""

    def test_config_creation_with_defaults(self):
        """Test creating config with default values"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
        )

        config = PlayHTTTSConfig(params=params)

        self.assertFalse(config.dump)
        self.assertIsInstance(config.dump_path, str)
        self.assertIn("playht_tts_in.pcm", config.dump_path)
        self.assertEqual(config.params, params)

    def test_config_creation_with_custom_values(self):
        """Test creating config with custom values"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            voice_engine="PlayDialog",
            format=Format.FORMAT_PCM,
            sample_rate=16000,
        )

        config = PlayHTTTSConfig(
            dump=True, dump_path="/custom/path/test.pcm", params=params
        )

        self.assertTrue(config.dump)
        self.assertEqual(config.dump_path, "/custom/path/test.pcm")
        self.assertEqual(config.params, params)

    def test_config_validation_with_invalid_params(self):
        """Test config validation with invalid params"""
        with self.assertRaises(ValidationError):
            PlayHTTTSConfig(
                dump=True,
                dump_path="/custom/path/test.pcm",
                params="invalid_params",  # Should be PlayHTParams instance
            )

    def test_config_serialization(self):
        """Test config serialization to JSON"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            voice_engine="PlayDialog",
            format=Format.FORMAT_PCM,
            sample_rate=16000,
        )

        config = PlayHTTTSConfig(
            dump=True, dump_path="/custom/path/test.pcm", params=params
        )

        # Test model_dump_json
        config_json = config.model_dump_json()
        config_dict = json.loads(config_json)

        self.assertTrue(config_dict["dump"])
        self.assertEqual(config_dict["dump_path"], "/custom/path/test.pcm")
        self.assertIn("params", config_dict)

        # Test that sensitive fields are encrypted in JSON
        params_json = config_dict["params"]
        self.assertIn("***", params_json["api_key"])
        self.assertIn("***", params_json["user_id"])
        self.assertNotIn("test_api_key", params_json["api_key"])
        self.assertNotIn("test_user_id", params_json["user_id"])

    def test_config_with_different_formats(self):
        """Test config with different audio formats"""
        formats_to_test = [
            Format.FORMAT_PCM,
            Format.FORMAT_MP3,
            Format.FORMAT_WAV,
        ]

        for format_type in formats_to_test:
            with self.subTest(format=format_type):
                params = PlayHTParams(
                    api_key="test_api_key",
                    user_id="test_user_id",
                    format=format_type,
                )

                config = PlayHTTTSConfig(params=params)
                self.assertEqual(config.params.format, format_type)

    def test_config_with_different_languages(self):
        """Test config with different languages"""
        languages_to_test = [
            Language.ENGLISH,
            Language.SPANISH,
            Language.FRENCH,
            Language.GERMAN,
        ]

        for language in languages_to_test:
            with self.subTest(language=language):
                params = PlayHTParams(
                    api_key="test_api_key",
                    user_id="test_user_id",
                    language=language,
                )

                config = PlayHTTTSConfig(params=params)
                self.assertEqual(config.params.language, language)

    def test_config_with_different_sample_rates(self):
        """Test config with different sample rates"""
        sample_rates_to_test = [8000, 16000, 22050, 44100, 48000]

        for sample_rate in sample_rates_to_test:
            with self.subTest(sample_rate=sample_rate):
                params = PlayHTParams(
                    api_key="test_api_key",
                    user_id="test_user_id",
                    sample_rate=sample_rate,
                )

                config = PlayHTTTSConfig(params=params)
                self.assertEqual(config.params.sample_rate, sample_rate)

    def test_config_dump_path_handling(self):
        """Test config dump path handling"""
        # Test with relative path
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
        )

        config = PlayHTTTSConfig(
            dump=True, dump_path="relative/path/test.pcm", params=params
        )

        self.assertEqual(config.dump_path, "relative/path/test.pcm")

        # Test with absolute path
        config = PlayHTTTSConfig(
            dump=True, dump_path="/absolute/path/test.pcm", params=params
        )

        self.assertEqual(config.dump_path, "/absolute/path/test.pcm")

    def test_config_minimal_params(self):
        """Test config with minimal required parameters"""
        params = PlayHTParams(
            api_key="minimal_api_key",
            user_id="minimal_user_id",
        )

        config = PlayHTTTSConfig(params=params)

        self.assertFalse(config.dump)
        self.assertEqual(config.params.api_key, "minimal_api_key")
        self.assertEqual(config.params.user_id, "minimal_user_id")
        self.assertEqual(config.params.protocol, "ws")  # default
        self.assertEqual(config.params.format, Format.FORMAT_PCM)  # default
        self.assertEqual(config.params.sample_rate, 16000)  # default
        self.assertIsNone(config.params.voice_engine)  # default
        self.assertIsNone(config.params.language)  # default

    def test_config_comprehensive_params(self):
        """Test config with all parameters specified"""
        params = PlayHTParams(
            api_key="comprehensive_api_key",
            user_id="comprehensive_user_id",
            voice_engine="PlayDialog",
            protocol="ws",
            language=Language.ENGLISH,
            format=Format.FORMAT_WAV,
            sample_rate=44100,
        )

        config = PlayHTTTSConfig(
            dump=True, dump_path="/comprehensive/path/test.wav", params=params
        )

        self.assertTrue(config.dump)
        self.assertEqual(config.dump_path, "/comprehensive/path/test.wav")
        self.assertEqual(config.params.api_key, "comprehensive_api_key")
        self.assertEqual(config.params.user_id, "comprehensive_user_id")
        self.assertEqual(config.params.voice_engine, "PlayDialog")
        self.assertEqual(config.params.protocol, "ws")
        self.assertEqual(config.params.language, Language.ENGLISH)
        self.assertEqual(config.params.format, Format.FORMAT_WAV)
        self.assertEqual(config.params.sample_rate, 44100)


class TestConfigIntegration(unittest.TestCase):
    """Integration tests for config classes"""

    def test_config_with_string_format_and_language(self):
        """Test config with string format and language values"""
        params = PlayHTParams(
            api_key="test_api_key",
            user_id="test_user_id",
            format="FORMAT_PCM",  # String format
            language="ENGLISH",  # String language
        )

        config = PlayHTTTSConfig(params=params)

        # Should be converted to enum values
        self.assertEqual(config.params.format, Format.FORMAT_PCM)
        self.assertEqual(config.params.language, Language.ENGLISH)


if __name__ == "__main__":
    unittest.main()
