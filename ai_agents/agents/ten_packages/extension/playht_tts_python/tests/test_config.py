import unittest
import json
from pydantic import ValidationError

from ..config import PlayHTTTSConfig
from pyht.client import Format, Language


class TestPlayHTTTSConfig(unittest.TestCase):
    """Test PlayHTTTSConfig class"""

    def test_config_creation_with_defaults(self):
        """Test creating config with default values"""
        config = PlayHTTTSConfig(
            params={"api_key": "test_api_key", "user_id": "test_user_id"}
        )

        self.assertFalse(config.dump)
        self.assertIsInstance(config.dump_path, str)
        self.assertIn("playht_tts_in.pcm", config.dump_path)
        self.assertEqual(config.params["api_key"], "test_api_key")
        self.assertEqual(config.params["user_id"], "test_user_id")

    def test_config_creation_with_custom_values(self):
        """Test creating config with custom values"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "voice_engine": "PlayDialog",
            "sample_rate": 16000,
        }

        config = PlayHTTTSConfig(
            dump=True,
            dump_path="/custom/path/test.pcm",
            params=params,
        )

        self.assertTrue(config.dump)
        self.assertEqual(config.dump_path, "/custom/path/test.pcm")
        self.assertEqual(config.params["api_key"], "test_api_key")
        self.assertEqual(config.params["user_id"], "test_user_id")
        self.assertEqual(config.params["voice_engine"], "PlayDialog")
        self.assertEqual(config.params["sample_rate"], 16000)

    def test_config_validation_with_missing_api_key(self):
        """Test config validation with missing api_key"""
        with self.assertRaises(ValueError):
            config = PlayHTTTSConfig(
                params={"api_key": "", "user_id": "test_user_id"}
            )
            config.validate()

    def test_config_validation_with_missing_user_id(self):
        """Test config validation with missing user_id"""
        with self.assertRaises(ValueError):
            config = PlayHTTTSConfig(
                params={"api_key": "test_api_key", "user_id": ""}
            )
            config.validate()

    def test_config_serialization(self):
        """Test config serialization to JSON"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "voice_engine": "PlayDialog",
            "sample_rate": 16000,
        }

        config = PlayHTTTSConfig(
            dump=True,
            dump_path="/custom/path/test.pcm",
            params=params,
        )

        # Test model_dump_json
        config_json = config.model_dump_json()
        config_dict = json.loads(config_json)

        self.assertTrue(config_dict["dump"])
        self.assertEqual(config_dict["dump_path"], "/custom/path/test.pcm")
        self.assertIn("params", config_dict)

    def test_config_to_str_with_sensitive_handling(self):
        """Test config to_str with sensitive data handling"""
        config = PlayHTTTSConfig(
            params={"api_key": "test_api_key", "user_id": "test_user_id"}
        )
        config_str = config.to_str(sensitive_handling=True)

        # Check that sensitive fields are encrypted (encrypt uses "..." format)
        self.assertIn("...", config_str)
        self.assertNotIn("test_api_key", config_str)
        self.assertNotIn("test_user_id", config_str)

    def test_config_update_params_removes_format(self):
        """Test that update_params removes format from params"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "format": "FORMAT_MP3",  # Should be removed
        }

        config = PlayHTTTSConfig(params=params)
        config.update_params()

        # Format should be removed
        self.assertNotIn("format", config.params)

    def test_config_with_different_sample_rates(self):
        """Test config with different sample rates"""
        sample_rates_to_test = [8000, 16000, 22050, 44100, 48000]

        for sample_rate in sample_rates_to_test:
            with self.subTest(sample_rate=sample_rate):
                params = {
                    "api_key": "test_api_key",
                    "user_id": "test_user_id",
                    "sample_rate": sample_rate,
                }

                config = PlayHTTTSConfig(params=params)
                self.assertEqual(config.params["sample_rate"], sample_rate)

    def test_config_dump_path_handling(self):
        """Test config dump path handling"""
        # Test with relative path
        config = PlayHTTTSConfig(
            dump=True,
            dump_path="relative/path/test.pcm",
            params={"api_key": "test_api_key", "user_id": "test_user_id"},
        )

        self.assertEqual(config.dump_path, "relative/path/test.pcm")

        # Test with absolute path
        config = PlayHTTTSConfig(
            dump=True,
            dump_path="/absolute/path/test.pcm",
            params={"api_key": "test_api_key", "user_id": "test_user_id"},
        )

        self.assertEqual(config.dump_path, "/absolute/path/test.pcm")

    def test_config_minimal_params(self):
        """Test config with minimal required parameters"""
        config = PlayHTTTSConfig(
            params={"api_key": "minimal_api_key", "user_id": "minimal_user_id"}
        )

        self.assertFalse(config.dump)
        self.assertEqual(config.params["api_key"], "minimal_api_key")
        self.assertEqual(config.params["user_id"], "minimal_user_id")

    def test_config_comprehensive_params(self):
        """Test config with all parameters specified"""
        params = {
            "api_key": "comprehensive_api_key",
            "user_id": "comprehensive_user_id",
            "voice_engine": "PlayDialog",
            "protocol": "ws",
            "sample_rate": 44100,
            "voice": "s3://voice-cloning-zero-shot/voice/manifest.json",
            "speed": 1.0,
            "temperature": 0.7,
        }

        config = PlayHTTTSConfig(
            dump=True,
            dump_path="/comprehensive/path/test.pcm",
            params=params,
        )

        self.assertTrue(config.dump)
        self.assertEqual(config.dump_path, "/comprehensive/path/test.pcm")
        self.assertEqual(config.params["api_key"], "comprehensive_api_key")
        self.assertEqual(config.params["user_id"], "comprehensive_user_id")
        self.assertEqual(config.params["voice_engine"], "PlayDialog")
        self.assertEqual(config.params["protocol"], "ws")
        self.assertEqual(config.params["sample_rate"], 44100)


if __name__ == "__main__":
    unittest.main()
