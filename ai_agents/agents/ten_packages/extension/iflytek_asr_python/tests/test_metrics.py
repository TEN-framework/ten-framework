import asyncio

from .helpers import create_extension, data_as_dict, make_response


def test_ttfw_and_ttlw_are_reported_as_metrics_messages() -> None:
    async def run() -> None:
        extension, ten_env, _ = create_extension()
        now = asyncio.get_running_loop().time()
        extension.first_audio_time = now - 0.05
        extension.last_finalize_time = now - 0.02

        await extension.on_response(
            make_response(text="hello", final=False, language="en-US")
        )
        await extension.on_response(
            make_response(text="hello world", final=True, language="en-US")
        )

        metrics = [
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "metrics"
        ]
        metric_values = {
            key: value
            for message in metrics
            for key, value in message["metrics"].items()
        }
        assert 40 <= metric_values["ttfw"] < 300
        assert 10 <= metric_values["ttlw"] < 300
        assert all(message["module"] == "asr" for message in metrics)
        assert all(message["vendor"] == "iflytek" for message in metrics)

    asyncio.run(run())
