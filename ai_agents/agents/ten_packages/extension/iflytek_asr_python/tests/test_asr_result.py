import asyncio

from .helpers import create_extension, data_as_dict, make_response


def test_interim_and_final_results_have_standard_fields_and_consistent_id() -> (
    None
):
    async def run() -> None:
        extension, ten_env, _ = create_extension()
        extension.metadata = {"session_id": "session-1"}

        await extension.on_response(
            make_response(text="hello", final=False, language="en-US")
        )
        await extension.on_response(
            make_response(text="hello world", final=True, language="en-US")
        )

        results = [
            data_as_dict(data)
            for data in ten_env.data
            if data.get_name() == "asr_result"
        ]
        assert len(results) == 2
        assert [result["final"] for result in results] == [False, True]
        assert results[0]["id"] == results[1]["id"]
        for result in results:
            assert {
                "id",
                "text",
                "final",
                "start_ms",
                "duration_ms",
                "language",
                "words",
                "metadata",
            } <= result.keys()
            assert result["language"] == "en-US"
            assert result["metadata"]["session_id"] == "session-1"
            assert result["metadata"]["asr_info"]["vendor"] == "iflytek"
            assert len(result["words"]) == 1
            assert {
                "word",
                "start_ms",
                "duration_ms",
                "stable",
            } <= result[
                "words"
            ][0].keys()
        assert results[0]["words"][0]["stable"] is False
        assert results[1]["words"][0]["stable"] is True

    asyncio.run(run())


def test_sensitive_temporary_voiceprints_are_not_forwarded() -> None:
    async def run() -> None:
        extension, ten_env, _ = create_extension()
        secret_voiceprint = "very-large-sensitive-biometric-template"

        await extension.on_response(
            make_response(
                text="hello",
                final=True,
                temporary_voiceprints={"speaker-1": secret_voiceprint},
            )
        )

        result = data_as_dict(
            next(
                data for data in ten_env.data if data.get_name() == "asr_result"
            )
        )
        asr_info = result["metadata"]["asr_info"]
        assert secret_voiceprint not in str(result)
        assert "temporary_voiceprints" not in asr_info
        assert asr_info["temporary_voiceprint_ids"] == ["speaker-1"]

    asyncio.run(run())
