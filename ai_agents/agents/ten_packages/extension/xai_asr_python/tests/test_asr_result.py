import asyncio
from unittest.mock import AsyncMock, MagicMock

from xai_asr_python.config import XAIASRConfig
from xai_asr_python.extension import XAIASRExtension


class FakeTimeline:
    def get_audio_duration_before_time(self, value: int) -> int:
        return value


def test_asr_result_shape():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.audio_timeline = FakeTimeline()
        extension.config = XAIASRConfig(
            params={"api_key": "xai-test-key", "language": "en"}
        )
        extension.config.apply_defaults()
        extension.metadata = {"session_id": "session-123", "turn_id": 9}
        extension.send_asr_result = AsyncMock()

        await extension._emit_asr_result(
            {
                "text": "hello world",
                "start": 0.12,
                "duration": 0.34,
                "words": [
                    {"text": "hello", "start": 0.12, "end": 0.2},
                    {"text": "world", "start": 0.2, "end": 0.34},
                ],
            },
            final=True,
            locked=False,
        )

        result = extension.send_asr_result.await_args.args[0]
        assert result.id
        assert result.text == "hello world"
        assert result.final is True
        assert result.start_ms == 120
        assert result.duration_ms == 340
        assert result.language == "en-US"
        assert result.metadata["session_id"] == "session-123"
        assert result.metadata["asr_info"]["vendor"] == "xai"
        assert len(result.words) == 2

    asyncio.run(_run())


def test_partial_final_mapping_sets_locked():
    async def _run():
        extension = XAIASRExtension("xai_asr_python")
        extension.ten_env = MagicMock()
        extension.audio_timeline = FakeTimeline()
        extension.config = XAIASRConfig(
            params={"api_key": "xai-test-key", "language": "en"}
        )
        extension.config.apply_defaults()
        extension.metadata = {"session_id": "session-123"}
        extension.send_asr_result = AsyncMock()

        await extension.on_partial_result(
            {
                "text": "hello",
                "start": 0.1,
                "duration": 0.2,
                "is_final": True,
                "speech_final": False,
            }
        )

        result = extension.send_asr_result.await_args.args[0]
        assert result.final is False
        assert result.metadata["asr_info"]["locked"] is True

    asyncio.run(_run())
