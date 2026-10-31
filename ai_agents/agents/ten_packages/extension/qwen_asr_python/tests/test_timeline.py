import asyncio
from unittest.mock import AsyncMock, MagicMock

import pytest
from ten_runtime import AudioFrame

from qwen_asr_python.config import QwenASRConfig
from qwen_asr_python.extension import QwenASRExtension


def make_client():
    client = MagicMock()
    client.connected = True
    client.task_ready = True
    client.connect = AsyncMock()
    client.close = AsyncMock()
    client.send_audio = AsyncMock()

    async def start_task():
        client.task_ready = True

    async def finish_task():
        client.task_ready = False

    client.start_task = AsyncMock(side_effect=start_task)
    client.finish_task = AsyncMock(side_effect=finish_task)
    return client


@pytest.fixture
def extension(monkeypatch):
    instance = QwenASRExtension("qwen_asr_python")
    instance.ten_env = MagicMock()
    instance.ten_env.send_data = AsyncMock()
    instance.config = QwenASRConfig(
        params={"api_key": "test-key", "language_hints": ["en"]}
    )
    instance.config.apply_defaults()
    instance.config.validate_config()
    instance.send_asr_result = AsyncMock()
    instance.send_asr_error = AsyncMock()
    instance.send_asr_finalize_end = AsyncMock()
    instance.send_connect_delay_metrics = AsyncMock()
    instance._send_asr_metrics = AsyncMock()
    monkeypatch.setattr(
        "qwen_asr_python.extension.QwenASRClient",
        MagicMock(side_effect=lambda **kwargs: make_client()),
    )
    return instance


def audio_frame(duration_ms):
    frame = AudioFrame.create("pcm_frame")
    frame.alloc_buf(duration_ms * 32)
    return frame


def result_event(begin_time=100, end_time=600, word_start=150, word_end=300):
    return {
        "payload": {
            "output": {
                "sentence": {
                    "text": "hello",
                    "sentence_end": True,
                    "begin_time": begin_time,
                    "end_time": end_time,
                    "words": [
                        {
                            "text": "hello",
                            "begin_time": word_start,
                            "end_time": word_end,
                        }
                    ],
                }
            }
        }
    }


def reported_metrics(extension):
    return [
        call.args[0].metrics
        for call in extension._send_asr_metrics.await_args_list
    ]


def test_finalize_preserves_cumulative_metrics_and_next_task_timestamps(
    extension,
):
    async def run():
        timeline = extension.audio_timeline
        await extension.start_connection()
        client = extension.client

        for turn in range(3):
            assert await extension.send_audio(audio_frame(1000), "session-1")
            await extension._handle_result(result_event())
            result = extension.send_asr_result.await_args.args[0]
            assert result.start_ms == turn * 1250 + 100
            assert result.duration_ms == 500
            assert result.words[0].start_ms == turn * 1250 + 150
            assert result.words[0].duration_ms == 150
            await extension._send_audio_actual_send_metrics()
            assert await extension.send_audio(audio_frame(250), "session-1")
            await extension.finalize("session-1")
            await extension._send_audio_actual_send_metrics()
            assert extension.audio_timeline is timeline
            assert timeline.total_user_audio_duration == (turn + 1) * 1250

        metrics = reported_metrics(extension)
        assert [item["actual_send"] for item in metrics] == [
            1000,
            1250,
            2250,
            2500,
            3500,
            3750,
        ]
        assert [item["actual_send_delta"] for item in metrics] == [
            1000,
            250,
        ] * 3
        assert sum(item["actual_send_delta"] for item in metrics) == 3750
        assert extension.client is client
        assert client.start_task.await_count == 2
        assert client.finish_task.await_count == 3
        client.close.assert_not_awaited()
        assert extension.send_asr_finalize_end.await_count == 3

    asyncio.run(run())


@pytest.mark.parametrize(
    "error",
    [
        TimeoutError("finish timeout"),
        RuntimeError("finish failed"),
        asyncio.CancelledError(),
    ],
)
def test_failed_finalize_preserves_timeline_and_metric_baseline(
    extension, error
):
    async def run():
        await extension.start_connection()
        assert await extension.send_audio(audio_frame(1000), "session-1")
        await extension._send_audio_actual_send_metrics()
        assert await extension.send_audio(audio_frame(750), "session-1")
        extension.client.finish_task.side_effect = error

        if isinstance(error, asyncio.CancelledError):
            with pytest.raises(asyncio.CancelledError):
                await extension.finalize("session-1")
        else:
            await extension.finalize("session-1")

        assert extension.audio_timeline.total_user_audio_duration == 1750
        assert extension.last_reported_audio_duration == 1000
        assert extension._timeline_time(100) == 100
        await extension._send_audio_actual_send_metrics()
        assert reported_metrics(extension)[-1] == {
            "actual_send": 1750,
            "actual_send_delta": 750,
        }
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(run())


def test_disconnected_finalize_does_not_change_timeline(extension):
    async def run():
        await extension.start_connection()
        assert await extension.send_audio(audio_frame(1000), "session-1")
        await extension._send_audio_actual_send_metrics()
        client = extension.client
        extension.connected = False
        await extension.finalize("session-1")
        assert extension.audio_timeline.total_user_audio_duration == 1000
        assert extension.last_reported_audio_duration == 1000
        assert extension._timeline_time(100) == 100
        client.finish_task.assert_not_awaited()
        extension.send_asr_finalize_end.assert_awaited_once()

    asyncio.run(run())


def test_reconnection_preserves_metrics_and_sets_new_task_origin(extension):
    async def run():
        await extension.start_connection()
        original_client = extension.client
        assert await extension.send_audio(audio_frame(1000), "session-1")
        await extension._send_audio_actual_send_metrics()
        assert await extension.send_audio(audio_frame(250), "session-1")
        await extension.start_connection()
        assert extension.client is not original_client
        original_client.close.assert_awaited_once()
        assert await extension.send_audio(audio_frame(750), "session-1")
        await extension._handle_result(result_event())
        result = extension.send_asr_result.await_args.args[0]
        assert result.start_ms == 1350
        assert result.words[0].start_ms == 1400
        await extension._send_audio_actual_send_metrics()
        assert reported_metrics(extension)[-1] == {
            "actual_send": 2000,
            "actual_send_delta": 1000,
        }

    asyncio.run(run())


def test_new_task_origin_is_committed_only_after_success(extension):
    async def run():
        await extension.start_connection()
        assert await extension.send_audio(audio_frame(1000), "session-1")
        await extension.finalize("session-1")
        client = extension.client
        start_task = client.start_task.side_effect
        client.start_task.side_effect = RuntimeError("task start failed")
        assert not await extension.send_audio(audio_frame(500), "session-1")
        assert extension._timeline_time(100) == 100
        assert extension.audio_timeline.total_user_audio_duration == 1000
        client.start_task.side_effect = start_task
        assert await extension.send_audio(audio_frame(500), "session-1")
        assert extension._timeline_time(100) == 1100
        assert extension.audio_timeline.total_user_audio_duration == 1500

    asyncio.run(run())


def test_task_timestamps_map_through_global_silence_and_dropped_audio(
    extension,
):
    async def run():
        timeline = extension.audio_timeline
        timeline.add_user_audio(1000)
        timeline.add_silence_audio(500)
        timeline.add_dropped_audio(300)
        await extension.start_connection()
        assert await extension.send_audio(audio_frame(200), "session-1")
        timeline.add_silence_audio(300)
        assert await extension.send_audio(audio_frame(500), "session-1")
        await extension._handle_result(result_event(word_end=550))
        result = extension.send_asr_result.await_args.args[0]
        assert result.start_ms == 1400
        assert result.duration_ms == 200
        assert result.words[0].start_ms == 1450
        assert result.words[0].duration_ms == 100
        await extension._send_audio_actual_send_metrics()
        assert reported_metrics(extension)[-1] == {
            "actual_send": 2500,
            "actual_send_delta": 2500,
        }

    asyncio.run(run())
