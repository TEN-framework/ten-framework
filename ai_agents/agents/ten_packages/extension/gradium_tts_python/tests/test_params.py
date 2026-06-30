import asyncio
import json
from unittest.mock import AsyncMock, MagicMock, patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from gradium_tts_python.config import GradiumTTSConfig
from gradium_tts_python.gradium_tts import GradiumTTSClient

from .gradium_mocks import make_streaming_mock_client


def create_mock_client():
    return make_streaming_mock_client(
        audio_chunks=(b"\x00\x01\x02\x03" * 100,), ttfb_ms=100
    )


def _capture_setup_payload(config: GradiumTTSConfig) -> dict:
    """Run _send_setup against a mock socket and return the sent payload."""

    async def _run():
        sent: list = []
        client = GradiumTTSClient(config=config, ten_env=MagicMock())

        async def _capture(payload):
            sent.append(payload)

        client._send_json = AsyncMock(side_effect=_capture)
        await client._send_setup()
        return sent[0]

    return asyncio.run(_run())


def test_params_passthrough():
    config = GradiumTTSConfig(
        params={
            "api_key": "test_api_key",
            "url": "wss://api.gradium.ai/api/speech/tts",
            "voice_id": "cLONiZ4hQ8VpQ4Sz",
            "sample_rate": 16000,
            "json_config": '{"speed": 1.1}',
            "close_ws_on_eos": False,
            "emotion": "calm",
        }
    )
    config.update_params()
    config.validate()

    assert config.websocket_url() == "wss://api.gradium.ai/api/speech/tts"
    assert config.output_format == "pcm_16000"
    assert config.get_sample_rate() == 16000
    # json_config stays a string on the model (matches the manifest schema).
    assert config.json_config == '{"speed": 1.1}'

    payload = _capture_setup_payload(config)

    # json_config is parsed into an object before being sent on the wire.
    assert payload["json_config"] == {"speed": 1.1}
    # vendor extras pass through; api_key is never sent in setup.
    assert payload["emotion"] == "calm"
    assert payload["close_ws_on_eos"] is False
    assert "api_key" not in payload


def test_json_config_invalid_json_is_sent_as_is():
    config = GradiumTTSConfig(
        params={
            "api_key": "test_api_key",
            "voice_id": "cLONiZ4hQ8VpQ4Sz",
            "json_config": "not-json",
        }
    )
    config.update_params()
    config.validate()

    payload = _capture_setup_payload(config)
    assert payload["json_config"] == "not-json"


def test_output_format_derived_from_sample_rate_only():
    """Gradium only supports PCM: sample_rate is the single source of truth.
    A user-supplied output_format must be ignored (neither override the
    derived pcm_<rate> nor leak into the vendor params as a duplicate)."""
    config = GradiumTTSConfig(
        params={
            "api_key": "test_api_key",
            "voice_id": "cLONiZ4hQ8VpQ4Sz",
            "sample_rate": 24000,
            # Conflicting / unsupported on purpose — must be dropped.
            "output_format": "pcm_16000",
        }
    )
    config.update_params()
    config.validate()

    # sample_rate wins; output_format is derived from it, not the input.
    assert config.output_format == "pcm_24000"
    assert config.get_sample_rate() == 24000
    # output_format is not carried through as a vendor passthrough param.
    assert "output_format" not in config.params


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
