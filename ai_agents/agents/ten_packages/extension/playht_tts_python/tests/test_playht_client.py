import unittest
from unittest.mock import MagicMock, patch
from pyht.client import TTSOptions, Language, Format

from ..config import PlayHTTTSConfig
from ..playht_tts import PlayHTTTSClient


class TestPlayHTParams(unittest.TestCase):
    """Test PlayHT params dict behavior (equivalent to old PlayHTParams class)"""

    def test_playht_params_creation(self):
        """Test creating params dict with valid data"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "voice_engine": "PlayDialog",
            "protocol": "ws",
            "sample_rate": 16000,
            "voice": "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        }

        config = PlayHTTTSConfig(params=params)

        self.assertEqual(config.params["api_key"], "test_api_key")
        self.assertEqual(config.params["user_id"], "test_user_id")
        self.assertEqual(config.params["voice_engine"], "PlayDialog")
        self.assertEqual(config.params["protocol"], "ws")
        self.assertEqual(config.params["sample_rate"], 16000)

    def test_playht_params_defaults(self):
        """Test params dict with default values"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "voice": "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        }

        config = PlayHTTTSConfig(params=params)
        config.update_params()  # This sets default protocol to "ws"

        self.assertEqual(config.params.get("protocol"), "ws")
        self.assertEqual(config.params.get("sample_rate"), None)  # Not set
        self.assertIsNone(config.params.get("voice_engine"))
        self.assertIsNone(config.params.get("language"))

        # Required params should be present
        self.assertEqual(config.params["api_key"], "test_api_key")
        self.assertEqual(config.params["user_id"], "test_user_id")
        self.assertEqual(
            config.params["voice"],
            "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        )

    def test_playht_params_format_validation(self):
        """Test format handling in params - format is removed by update_params"""
        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "format": "FORMAT_PCM",  # This will be removed by update_params
            "voice": "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        }

        config = PlayHTTTSConfig(params=params)
        config.update_params()

        # Format should be removed (forced to PCM internally)
        self.assertNotIn("format", config.params)

    @patch("playht_tts_python.playht_tts.AsyncClient")
    def test_playht_params_to_tts_options(self, mock_async_client):
        """Test conversion to TTSOptions in client"""
        mock_client_instance = MagicMock()
        mock_async_client.return_value = mock_client_instance

        params = {
            "api_key": "test_api_key",
            "user_id": "test_user_id",
            "voice_engine": "PlayDialog",
            "protocol": "ws",
            "sample_rate": 16000,
            "language": "ENGLISH",
            "voice": "s3://voice-cloning-zero-shot/d9ff78ba-d016-47f6-b0ef-dd630f59414e/female-cs/manifest.json",
        }

        config = PlayHTTTSConfig(params=params)
        config.update_params()

        ten_env = MagicMock()
        ten_env.log_debug = MagicMock()
        ten_env.log_error = MagicMock()
        ten_env.log_warning = MagicMock()

        client = PlayHTTTSClient(config=config, ten_env=ten_env)

        # The client should convert params to TTSOptions internally
        # We can verify this by checking that the client can be created
        # and that it uses the params correctly
        self.assertIsNotNone(client.client)
        self.assertEqual(client.config.params["sample_rate"], 16000)
        self.assertEqual(client.config.params["language"], "ENGLISH")


if __name__ == "__main__":
    unittest.main()
