import asyncio
import json

from ten_runtime import Data

from .helpers import (
    FakeClient,
    create_config,
    create_extension,
    data_as_dict,
    make_response,
)


def create_finalize_data(finalize_id: str, session_id: str) -> Data:
    data = Data.create("asr_finalize")
    data.set_property_from_json(
        None,
        json.dumps(
            {
                "finalize_id": finalize_id,
                "metadata": {"session_id": session_id},
            }
        ),
    )
    return data


def test_finalize_data_handles_inline_terminal_response_without_race() -> None:
    async def run() -> None:
        extension, ten_env, _ = create_extension()
        extension.metadata = {"session_id": "session-1"}

        async def respond_during_finalize() -> None:
            await extension.on_response(
                make_response(text="hello", final=True, terminal=True)
            )

        client = FakeClient(on_finalize=respond_during_finalize)
        extension.client = client  # type: ignore[assignment]
        started_at = asyncio.get_running_loop().time()

        await extension.on_data(
            ten_env, create_finalize_data("finalize-1", "session-1")
        )
        elapsed_ms = (asyncio.get_running_loop().time() - started_at) * 1000

        assert elapsed_ms < 300
        assert client.finalized is True
        assert [data.get_name() for data in ten_env.data].count(
            "asr_finalize_end"
        ) == 1
        finalize_end = next(
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "asr_finalize_end"
        )
        assert finalize_end == {
            "finalize_id": "finalize-1",
            "metadata": {"session_id": "session-1"},
        }

    asyncio.run(run())


def test_finalize_timeout_reports_nonfatal_and_completes_once() -> None:
    async def run() -> None:
        config = create_config(finalize_timeout=0.01)
        extension, ten_env, _ = create_extension(config=config)
        extension.metadata = {"session_id": "session-1"}

        await extension.on_data(
            ten_env, create_finalize_data("finalize-timeout", "session-1")
        )
        await asyncio.sleep(0.03)

        errors = [
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "error"
        ]
        assert len(errors) == 1
        assert errors[0]["code"] == 1000
        assert errors[0]["vendor_info"]["code"] == "finalize_timeout"
        assert [data.get_name() for data in ten_env.data].count(
            "asr_finalize_end"
        ) == 1

        await extension.on_response(
            make_response(text="late", final=True, terminal=True)
        )
        assert [data.get_name() for data in ten_env.data].count(
            "asr_finalize_end"
        ) == 1

    asyncio.run(run())
