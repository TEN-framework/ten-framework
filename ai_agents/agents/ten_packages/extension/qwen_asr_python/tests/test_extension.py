import asyncio
from unittest.mock import AsyncMock, MagicMock

from qwen_asr_python.config import QwenASRConfig
from qwen_asr_python.extension import QwenASRExtension


class FakeTimeline:
    total_user_audio_duration = 0

    def get_audio_duration_before_time(self, value):
        return value

    def reset(self):
        pass


def configured_extension():
    extension = QwenASRExtension("qwen_asr_python")
    extension.ten_env = MagicMock()
    extension.audio_timeline = FakeTimeline()
    extension.config = QwenASRConfig(
        params={"api_key": "secret", "language_hints": ["zh"]}
    )
    extension.config.apply_defaults()
    extension.config.validate_config()
    extension.metadata = {"session_id": "session-1", "turn_id": 7}
    extension.send_asr_result = AsyncMock()
    return extension


def test_result_maps_sentence_words_and_metadata():
    async def run():
        extension = configured_extension()
        await extension._handle_result(
            {
                "payload": {
                    "output": {
                        "sentence": {
                            "begin_time": 160,
                            "end_time": 1640,
                            "text": "欢迎使用阿里云。",
                            "sentence_end": True,
                            "sentence_id": 1,
                            "words": [
                                {
                                    "begin_time": 160,
                                    "end_time": 520,
                                    "text": "欢迎",
                                    "fixed": True,
                                },
                                {
                                    "begin_time": 520,
                                    "end_time": 880,
                                    "text": "使用",
                                    "fixed": True,
                                },
                            ],
                        }
                    },
                    "request_id": "request-1",
                }
            }
        )
        result = extension.send_asr_result.await_args.args[0]
        assert result.text == "欢迎使用阿里云。"
        assert result.final is True
        assert result.start_ms == 160
        assert result.duration_ms == 1480
        assert result.language == "zh-CN"
        assert result.metadata["session_id"] == "session-1"
        assert result.metadata["asr_info"]["request_id"] == "request-1"
        assert result.words[0].stable is True

    asyncio.run(run())


def test_empty_and_heartbeat_results_are_ignored():
    async def run():
        extension = configured_extension()
        await extension._handle_result({"payload": {"heartbeat": True}})
        await extension._handle_result(
            {"payload": {"output": {"sentence": {"text": ""}}}}
        )
        extension.send_asr_result.assert_not_awaited()

    asyncio.run(run())
