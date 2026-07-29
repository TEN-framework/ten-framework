from unittest.mock import ANY, MagicMock, patch

from ..config import CosyTTSConfig
from ..cosy_tts import AUDIO_FORMAT_MAPPING, CosyTTSClient


@patch("cosy_tts_python.cosy_tts.SpeechSynthesizer")
def test_url_and_vendor_request_id_logging(mock_synthesizer_cls):
    ten_env = MagicMock()
    synthesizer = mock_synthesizer_cls.return_value
    synthesizer.get_last_request_id.return_value = "vendor-request-id"

    config = CosyTTSConfig(
        api_key="test-key",
        model="cosyvoice-v1",
        voice="longxiaochun",
        url="wss://example.com/api-ws/v1/inference",
    )
    client = CosyTTSClient(config, ten_env, "cosy")
    client.start()

    mock_synthesizer_cls.assert_called_once_with(
        callback=ANY,
        format=AUDIO_FORMAT_MAPPING[16000],
        model="cosyvoice-v1",
        voice="longxiaochun",
        url="wss://example.com/api-ws/v1/inference",
    )

    client.synthesize_audio("hello", False)
    client.synthesize_audio("world", True)

    request_logs = [
        call
        for call in ten_env.log_info.call_args_list
        if "vendor_request_id: vendor-request-id" in call.args[0]
    ]
    assert len(request_logs) == 2


@patch("cosy_tts_python.cosy_tts.SpeechSynthesizer")
def test_empty_url_uses_sdk_default(mock_synthesizer_cls):
    config = CosyTTSConfig(
        api_key="test-key",
        model="cosyvoice-v1",
        voice="longxiaochun",
    )
    client = CosyTTSClient(config, MagicMock(), "cosy")
    client.start()

    assert mock_synthesizer_cls.call_args.kwargs["url"] is None
