import asyncio
import pytest
from unittest.mock import MagicMock, AsyncMock, patch
from ten_ai_base.struct import ASRResult


@pytest.fixture(scope="function")
def patch_google_asr_client():
    """Patch GoogleASRClient used by the extension to a fake async client.

    The fake client immediately emits a final ASRResult after start() to
    drive the extension/tester flow without real network calls.
    """

    patch_target = (
        "ten_packages.extension.google_asr_python.extension.GoogleASRClient"
    )

    def _fake_ctor(config, ten_env, on_result_callback, on_error_callback):
        class _FakeClient:
            async def start(self, session_id: str | None):
                print("[mock] GoogleASRClient.start called")

                async def _emit_result_later():
                    # Give tester's audio sender time and avoid blocking start()
                    await asyncio.sleep(1.0)
                    result = ASRResult(
                        final=True,
                        text="hello world",
                        words=[],
                        confidence=0.95,
                        language="en-US",
                        start_ms=0,
                        duration_ms=500,
                    )
                    print("[mock] emitting final asr_result")
                    await on_result_callback(result)

                asyncio.create_task(_emit_result_later())
                print("[mock] start returning immediately")
                return None

            stop = AsyncMock()
            send_audio = AsyncMock()
            finalize = AsyncMock()

        return _FakeClient()

    with patch(patch_target) as MockClient:
        MockClient.side_effect = _fake_ctor
        yield MockClient
