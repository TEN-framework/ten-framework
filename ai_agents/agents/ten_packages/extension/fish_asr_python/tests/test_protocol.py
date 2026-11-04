import pytest

from ..config import FishAudioASRConfig
from ..websocket import pack_message, unpack_message


def test_config_uses_native_fish_realtime_protocol():
    config = FishAudioASRConfig(api_key="secret")
    message = config.build_config_message()

    assert message == {
        "type": "config",
        "audio": {
            "format": "pcm_s16le",
            "sample_rate": 16000,
            "channels": 1,
        },
        "turn_detection": {"mode": "auto", "eager": True},
        "results": {
            "drafts": True,
            "ignore_timestamps": False,
            "diarize": False,
            "vad_events": False,
            "eot_events": False,
        },
    }


def test_audio_is_a_messagepack_binary_value():
    encoded = pack_message({"type": "audio", "data": b"\x00\x01"})
    decoded = unpack_message(encoded)

    assert isinstance(encoded, bytes)
    assert decoded == {"type": "audio", "data": b"\x00\x01"}


@pytest.mark.parametrize("turn_mode", ["auto", "vad", "manual"])
def test_turn_mode_is_forwarded(turn_mode):
    config = FishAudioASRConfig(api_key="secret", turn_mode=turn_mode)
    message = config.build_config_message()
    assert message["turn_detection"]["mode"] == turn_mode
    if turn_mode != "auto":
        assert "eager" not in message["turn_detection"]


def test_context_and_optional_settings_are_forwarded():
    config = FishAudioASRConfig(
        api_key="secret",
        sample_rate=8000,
        drafts=False,
        silence_timeout_ms=3000,
        context_history=[{"role": "assistant", "text": "Hello"}],
        context_terms=["TEN Framework"],
        client_reference_id="test-session",
    )
    message = config.build_config_message()

    assert message["audio"]["sample_rate"] == 8000
    assert message["results"]["drafts"] is False
    assert message["turn_detection"]["silence_timeout_ms"] == 3000
    assert message["context"]["history"][0]["text"] == "Hello"
    assert message["context"]["terms"] == ["TEN Framework"]
    assert message["client_reference_id"] == "test-session"


def test_invalid_protocol_fields_are_rejected_before_connecting():
    with pytest.raises(ValueError, match="silence_timeout_ms"):
        FishAudioASRConfig(
            api_key="secret", turn_mode="auto", silence_timeout_ms=700
        ).validate_config({16000})
    with pytest.raises(ValueError, match="model"):
        FishAudioASRConfig(
            api_key="secret", model="transcribe-1"
        ).validate_config({16000})


def test_config_redacts_keys_only_when_requested():
    config = FishAudioASRConfig(
        api_key="secret", params={"api_key": "nested-secret"}
    )
    assert "secret" in config.to_str(sensitive_handling=False)
    assert "nested-secret" in config.to_str(sensitive_handling=False)
    assert "secret" not in config.to_str(sensitive_handling=True)
    assert "nested-secret" not in config.to_str(sensitive_handling=True)


def test_config_urls_redact_userinfo_and_signed_queries():
    url = (
        "wss://url-user:url-password@example.test/realtime?"
        "token=signed-token&X-Amz-Signature=signature-value&region=us"
    )
    config = FishAudioASRConfig(
        api_key="secret", url=url, params={"endpoint_url": url}
    )
    redacted = config.to_str(sensitive_handling=True)

    assert "url-user" not in redacted
    assert "url-password" not in redacted
    assert "signed-token" not in redacted
    assert "signature-value" not in redacted
    assert "region=us" in redacted
