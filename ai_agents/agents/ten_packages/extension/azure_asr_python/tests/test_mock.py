#
# Copyright © 2024 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
import json
import threading
from types import SimpleNamespace
from unittest.mock import MagicMock

from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
    Data,
    TenError,
    TenErrorCode,
)

# We must import it, which means this test fixture will be automatically executed
from .mock import patch_azure_ws  # noqa: F401


class ExtensionTesterAzure(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.stopped = False

    async def audio_sender(self, ten_env: AsyncTenEnvTester):
        while not self.stopped:
            chunk = b"\x01\x02" * 160  # 320 bytes (16-bit * 160 samples)
            if not chunk:
                break
            audio_frame = AudioFrame.create("pcm_frame")
            audio_frame.set_property_int("stream_id", 123)
            audio_frame.set_property_string("remote_user_id", "123")
            audio_frame.alloc_buf(len(chunk))
            buf = audio_frame.lock_buf()
            buf[:] = chunk
            audio_frame.unlock_buf(buf)
            await ten_env.send_audio_frame(audio_frame)
            await asyncio.sleep(0.1)

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        # Create a task to read pcm file and send to extension
        self.sender_task = asyncio.create_task(self.audio_sender(ten_env))

    async def on_data(self, ten_env: AsyncTenEnvTester, data: Data) -> None:
        name = data.get_name()

        ten_env.log_info(f"on_data name: {name}")
        if name == "asr_result":
            json_str, _ = data.get_property_to_json(None)

            json_data = json.loads(json_str)

            language = json_data.get("language", "")
            if language != "en-US":
                ten_env.log_error(f"language: {language}")
                ten_env.stop_test(
                    TenError.create(
                        TenErrorCode.ErrorCodeGeneric,
                        f"unexpected language: {language}",
                    )
                )
                return

            text = json_data.get("text", "")
            if text != "hello world":
                ten_env.log_error(f"text: {text}")
                ten_env.stop_test(
                    TenError.create(
                        TenErrorCode.ErrorCodeGeneric,
                        f"unexpected text: {text}",
                    )
                )
                return

            # Success
            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("Stopping audio sender task...")
        self.stopped = True
        self.sender_task.cancel()
        try:
            await self.sender_task
        except asyncio.CancelledError:
            ten_env.log_info("Audio sender task cancelled successfully")
        except Exception as e:
            ten_env.log_error(
                f"Error while cancelling audio sender task: {str(e)}"
            )
        finally:
            ten_env.log_info("Audio sender task cleanup completed")

        print("on_stop_done")


# def test_azure(patch_azure_ws):
#     def fake_start_continuous_recognition_async_get():

#         def triggerConnected():
#             evt = SimpleNamespace(
#                 result=SimpleNamespace(
#                     json=json.dumps(
#                         {
#                             "Connected": True,
#                         }
#                     )
#                 )
#             )
#             patch_azure_ws.event_handlers["connected"](evt)
#         threading.Timer(1.0, triggerConnected).start()

#         def triggerRecognized():
#             evt = SimpleNamespace(
#                 result=SimpleNamespace(
#                     json=json.dumps(
#                         {
#                             "DisplayText": "hello world",
#                             "Offset": 0,
#                             "Duration": 5000000,
#                         }
#                     )
#                 )
#             )
#             patch_azure_ws.event_handlers["recognized"](evt)

#         threading.Timer(2.0, triggerRecognized).start()
#         return None

#     start_future = MagicMock()
#     start_future.get.side_effect = fake_start_continuous_recognition_async_get

#     # Inject into recognizer
#     patch_azure_ws.recognizer_instance.start_continuous_recognition_async.return_value = (
#         start_future
#     )
#     stop_future = MagicMock()
#     stop_future.get.return_value = None
#     patch_azure_ws.recognizer_instance.stop_continuous_recognition_async.return_value = (
#         stop_future
#     )

#     tester = ExtensionTesterAzure()
#     tester.set_test_mode_single(
#         "azure_asr_python",
#         json.dumps(
#             {
#                 "api_key": "111",
#                 "language": "en-US",
#                 "model": "nova-2",
#                 "sample_rate": 16000,
#                 "params": {"test": "123"},
#             }
#         ),
#     )

#     error = tester.run()
#     assert error is None


# def test_azure_unexpected_result(patch_azure_ws):
#     def fake_start_continuous_recognition_async_get():

#         def triggerRecognized():
#             evt = SimpleNamespace(
#                 result=SimpleNamespace(
#                     json=json.dumps(
#                         {
#                             "DisplayText": "goodbye world",
#                             "Offset": 0,
#                             "Duration": 5000000,
#                         }
#                     )
#                 )
#             )
#             patch_azure_ws.event_handlers["recognized"](evt)

#         threading.Timer(1.0, triggerRecognized).start()
#         return None

#     start_future = MagicMock()
#     start_future.get.side_effect = fake_start_continuous_recognition_async_get

#     # Inject into recognizer
#     patch_azure_ws.recognizer_instance.start_continuous_recognition_async.return_value = (
#         start_future
#     )
#     stop_future = MagicMock()
#     stop_future.get.return_value = None
#     patch_azure_ws.recognizer_instance.stop_continuous_recognition_async.return_value = (
#         stop_future
#     )

#     tester = ExtensionTesterAzure()
#     tester.set_test_mode_single(
#         "azure_asr_python",
#         json.dumps(
#             {
#                 "api_key": "111",
#                 "language": "en-US",
#                 "model": "nova-2",
#                 "sample_rate": 16000,
#             }
#         ),
#     )

#     error = tester.run()
#     assert error is not None
#     assert error.error_code() == TenErrorCode.ErrorCodeGeneric
#     assert error.error_message() == "unexpected text: goodbye world"
