from unittest.mock import MagicMock, patch

# pylint: disable=protected-access

from ..config import CosyTTSConfig
from ..cosy_tts import AUDIO_FORMAT_MAPPING, SharedPool


def _reset_shared_pool() -> None:
    SharedPool._pool = None
    SharedPool._semaphore = None
    SharedPool._signature = None
    SharedPool._clients = 0


def test_pool_uses_custom_url_workspace_and_headers():
    _reset_shared_pool()
    pool = MagicMock()
    config = CosyTTSConfig(
        api_key="test-key",
        model="cosyvoice-v3-flash",
        voice="longanyang",
        url="wss://example.com/api-ws/v1/inference",
        workspace_id="workspace-id",
        headers={"X-Test": "value"},
        pool_size=2,
    )

    with patch(
        "cosy_tts_python.cosy_tts.SpeechSynthesizerObjectPool",
        return_value=pool,
    ) as pool_class:
        SharedPool.register(config)
        SharedPool.release_client()

    pool_class.assert_called_once_with(
        max_size=2,
        url="wss://example.com/api-ws/v1/inference",
        headers={
            "Authorization": "Bearer test-key",
            "User-Agent": "ten-cosy-tts/0.4.4",
            "X-Test": "value",
        },
        workspace="workspace-id",
    )
    pool.shutdown.assert_called_once()
    _reset_shared_pool()


def test_borrow_forwards_provider_params_and_pcm_sample_rate():
    _reset_shared_pool()
    pool = MagicMock()
    synthesizer = MagicMock()
    pool.borrow_synthesizer.return_value = synthesizer
    config = CosyTTSConfig(
        api_key="test-key",
        model="cosyvoice-v3-flash",
        voice="longanyang",
        sample_rate=24000,
        params={
            "api_key": "test-key",
            "model": "cosyvoice-v3-flash",
            "voice": "longanyang",
            "sample_rate": 24000,
            "instruction": "speak happily",
            "future_protocol_parameter": "future-value",
        },
    )
    config.update_params()

    with patch(
        "cosy_tts_python.cosy_tts.SpeechSynthesizerObjectPool",
        return_value=pool,
    ):
        SharedPool.register(config)
        callback = MagicMock()
        lease = SharedPool.borrow(config, callback)
        SharedPool.return_lease(lease)
        SharedPool.release_client()

    pool.borrow_synthesizer.assert_called_once_with(
        callback=callback,
        format=AUDIO_FORMAT_MAPPING[24000],
        model="cosyvoice-v3-flash",
        voice="longanyang",
        additional_params={
            "instruction": "speak happily",
            "future_protocol_parameter": "future-value",
        },
    )
    pool.return_synthesizer.assert_called_once_with(synthesizer)
    _reset_shared_pool()
