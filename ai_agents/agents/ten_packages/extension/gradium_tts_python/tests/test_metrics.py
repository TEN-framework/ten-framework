import json
from unittest.mock import patch

from ten_ai_base.struct import TTSTextInput
from ten_runtime import Data, ExtensionTester, TenEnvTester

from .gradium_mocks import make_streaming_mock_client


class ExtensionTesterMetrics(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.ttfb_received = False
        self.ttfb_value = -1
        self.ttfb_count = 0
        self.audio_frame_received = False
        self.audio_end_received = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        tts_input = TTSTextInput(
            request_id="tts_request_for_metrics",
            text="hello, this is a metrics test.",
            text_input_end=True,
        )
        data = Data.create("tts_text_input")
        data.set_property_from_json(None, tts_input.model_dump_json())
        ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data) -> None:
        name = data.get_name()
        if name == "metrics":
            json_str, _ = data.get_property_to_json(None)
            metrics_data = json.loads(json_str)
            nested_metrics = metrics_data.get("metrics", {})
            if "ttfb" in nested_metrics:
                self.ttfb_received = True
                self.ttfb_value = nested_metrics.get("ttfb", -1)
                self.ttfb_count += 1
        elif name == "tts_audio_end":
            self.audio_end_received = True
            if self.ttfb_received:
                ten_env.stop_test()

    def on_audio_frame(self, _ten_env: TenEnvTester, _audio_frame):
        self.audio_frame_received = True


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_ttfb_metric_is_sent(mock_client):
    mock_client.return_value = make_streaming_mock_client(
        audio_chunks=(b"\x11\x22\x33",),
        ttfb_ms=255,
        extra_metadata={"voice_id": "cLONiZ4hQ8VpQ4Sz"},
    )

    tester = ExtensionTesterMetrics()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()

    assert tester.audio_frame_received
    assert tester.audio_end_received
    assert tester.ttfb_received
    assert tester.ttfb_value == 255
    assert tester.ttfb_count == 1


class ExtensionTesterSegmentedMetrics(ExtensionTesterMetrics):
    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        first = TTSTextInput(
            request_id="tts_request_segmented_metrics",
            text=(
                "This is the first sentence streamed to the vendor "
                "immediately as it arrives."
            ),
            text_input_end=False,
        )
        second = TTSTextInput(
            request_id="tts_request_segmented_metrics",
            text=" This is the final sentence for the same request.",
            text_input_end=True,
        )

        for item in (first, second):
            data = Data.create("tts_text_input")
            data.set_property_from_json(None, item.model_dump_json())
            ten_env_tester.send_data(data)
        ten_env_tester.on_start_done()


@patch("gradium_tts_python.extension.GradiumTTSClient")
def test_ttfb_metric_is_only_sent_once_per_request(mock_client):
    """A multi-segment request shares one streaming session and emits a
    single ttfb metric, even though several text segments are forwarded."""
    mock_client.return_value = make_streaming_mock_client(
        audio_chunks=(b"\x11\x22\x33",),
        ttfb_ms=111,
        extra_metadata={"voice_id": "cLONiZ4hQ8VpQ4Sz"},
    )

    tester = ExtensionTesterSegmentedMetrics()
    tester.set_test_mode_single(
        "gradium_tts_python",
        json.dumps(
            {
                "params": {
                    "api_key": "test_api_key",
                    "voice_id": "cLONiZ4hQ8VpQ4Sz",
                }
            }
        ),
    )
    tester.run()

    assert tester.audio_frame_received
    assert tester.audio_end_received
    assert tester.ttfb_received
    assert tester.ttfb_count == 1
