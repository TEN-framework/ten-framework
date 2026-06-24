import json
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.gradium_tts import (
    EVENT_TTS_END,
    EVENT_TTS_RESPONSE,
    EVENT_TTS_TTFB_METRIC,
    GradiumTTSClient,
)


def create_mock_client():
    mock = MagicMock()
    mock.start = AsyncMock()
    mock.clean = AsyncMock()
    mock.cancel = AsyncMock()
    mock.get_ready_sample_rate.return_value = 24000
    mock.get_extra_metadata.return_value = {}
    fake_audio = b"\x00\x01\x02\x03" * 100

    async def mock_get(_text: str, *_args):
        yield 100, EVENT_TTS_TTFB_METRIC
        yield fake_audio, EVENT_TTS_RESPONSE
        yield None, EVENT_TTS_END

    mock.get.side_effect = mock_get
    return mock


def test_params_passthrough():
    config = GradiumTTSConfig(
        params={
            "api_key": "test_api_key",
            "base_url": "wss://api.gradium.ai/api/speech/tts",
            "voice_id": "cLONiZ4hQ8VpQ4Sz",
            "sample_rate": 16000,
            "json_config": {"speed": 1.1},
            "close_ws_on_eos": False,
            "emotion": "calm",
        }
    )
    config.update_params()
    config.validate()

    client = GradiumTTSClient(config=config, ten_env=MagicMock())
    payload = {
        "type": "setup",
        "model_name": config.model_name,
        "voice_id": config.voice_id,
        "output_format": config.output_format,
        "close_ws_on_eos": config.close_ws_on_eos,
        "json_config": config.json_config,
        **config.params,
    }

    assert config.websocket_url() == "wss://api.gradium.ai/api/speech/tts"
    assert config.output_format == "pcm_16000"
    assert config.get_sample_rate() == 16000
    assert payload["emotion"] == "calm"
    assert "api_key" not in payload
    assert client is not None


class ExtensionTesterSampleRate(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_end_received = False
        self.audio_chunks_count = 0

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="tts_request_sr",
            text="Testing different sample rates.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        if data.get_name() == "tts_audio_end":
            self.audio_end_received = True
            ten_env.stop_test()

    def on_audio_frame(self, _ten_env: TenEnvTester, _audio_frame):
        self.audio_chunks_count += 1


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_sample_rate_16000(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterSampleRate()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                    "sample_rate": 16000,
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received
    assert tester.audio_chunks_count > 0


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_sample_rate_24000(mock_client):
    mock_client.return_value = create_mock_client()
    tester = ExtensionTesterSampleRate()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                    "sample_rate": 24000,
                }
            }
        ),
    )
    tester.run()
    assert tester.audio_end_received
    assert tester.audio_chunks_count > 0
