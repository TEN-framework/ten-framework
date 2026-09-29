import asyncio
import json
import threading
from types import SimpleNamespace
from typing_extensions import override
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Data,
    AudioFrame,
    TenError,
    TenErrorCode,
)

from .mock import patch_azure_ws, trigger_vendor_live  # noqa: F401


class SameSessionFinalizeDisconnectTester(AsyncExtensionTester):
    """Two speak/finalize cycles with finalize_mode=disconnect in one session."""

    def __init__(self):
        super().__init__()
        self.cycle = 0
        self.final_texts: list[str] = []

    @override
    async def on_start(self, ten_env_tester: AsyncTenEnvTester) -> None:
        for cycle in (1, 2):
            self.cycle = cycle
            for _ in range(5):
                chunk = b"\x01\x02" * 160
                audio_frame = AudioFrame.create("pcm_frame")
                metadata = {"session_id": "123"}
                audio_frame.set_property_from_json(
                    "metadata", json.dumps(metadata)
                )
                audio_frame.alloc_buf(len(chunk))
                buf = audio_frame.lock_buf()
                buf[:] = chunk
                audio_frame.unlock_buf(buf)
                await ten_env_tester.send_audio_frame(audio_frame)
                await asyncio.sleep(0.05)

            finalize_data = Data.create("asr_finalize")
            finalize_data.set_property_from_json(
                None,
                json.dumps(
                    {
                        "finalize_id": str(cycle),
                        "metadata": {"session_id": "123"},
                    }
                ),
            )
            await ten_env_tester.send_data(finalize_data)
            await asyncio.sleep(1.0)

    def stop_test_if_checking_failed(
        self,
        ten_env_tester: AsyncTenEnvTester,
        success: bool,
        error_message: str,
    ) -> None:
        if not success:
            ten_env_tester.stop_test(
                TenError.create(
                    TenErrorCode.ErrorCodeGeneric, error_message
                )
            )

    @override
    async def on_data(
        self, ten_env_tester: AsyncTenEnvTester, data: Data
    ) -> None:
        data_name = data.get_name()
        if data_name != "asr_result":
            return

        data_json, _ = data.get_property_to_json()
        data_dict = json.loads(data_json)
        if not data_dict.get("final"):
            return

        text = data_dict.get("text", "")
        self.final_texts.append(text)
        if len(self.final_texts) >= 2:
            self.stop_test_if_checking_failed(
                ten_env_tester,
                self.final_texts[0] != "" and self.final_texts[1] != "",
                f"expected two non-empty finals, got: {self.final_texts}",
            )
            ten_env_tester.stop_test()


def test_same_session_finalize_disconnect_two_cycles(patch_azure_ws):
    start_calls = 0
    lock = threading.Lock()

    def fake_start_continuous_recognition():
        nonlocal start_calls
        with lock:
            start_calls += 1
            call_index = start_calls

        def emit_finalize_stop_then_reopen():
            stopped = SimpleNamespace(session_id="123")
            patch_azure_ws.event_handlers["session_stopped"](stopped)
            threading.Timer(
                0.15,
                lambda: trigger_vendor_live(
                    patch_azure_ws.event_handlers, session_id="123"
                ),
            ).start()

        def emit_result_and_finalize_stop():
            evt = SimpleNamespace(
                result=SimpleNamespace(
                    text=f"cycle-{call_index}",
                    offset=0,
                    duration=5000000,
                    no_match_details=None,
                    json=json.dumps(
                        {
                            "DisplayText": f"cycle-{call_index}",
                            "Offset": 0,
                            "Duration": 5000000,
                        }
                    ),
                )
            )
            patch_azure_ws.event_handlers["recognized"](evt)
            threading.Timer(0.05, emit_finalize_stop_then_reopen).start()

        threading.Timer(0.2, emit_result_and_finalize_stop).start()
        return None

    patch_azure_ws.recognizer_instance.start_continuous_recognition.side_effect = (
        fake_start_continuous_recognition
    )
    patch_azure_ws.recognizer_instance.stop_continuous_recognition.return_value = (
        None
    )

    property_json = {
        "params": {
            "key": "fake_key",
            "region": "fake_region",
            "finalize_mode": "disconnect",
        }
    }

    tester = SameSessionFinalizeDisconnectTester()
    tester.set_test_mode_single("azure_asr_python", json.dumps(property_json))
    err = tester.run()
    assert err is None, (
        f"test_same_session_finalize_disconnect_two_cycles failed: "
        f"{err.error_code() if err else None} {err.error_message() if err else ''}"
    )
    assert start_calls >= 3, f"expected reopen after finalize, starts={start_calls}"
