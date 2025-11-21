#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from types import SimpleNamespace
import pytest
from unittest.mock import MagicMock, patch, AsyncMock
import asyncio
import json


@pytest.fixture(scope="function")
def patch_volcengine_ws():
    """Mock Volcengine ASR WebSocket client and related components."""

    patch_target = "ten_packages.extension.bytedance_llm_based_asr.extension.VolcengineASRClient"

    def _fake_ctor(
        url,
        app_key,
        access_key,
        api_key,
        auth_method,
        config,
        ten_env=None,
        audio_timeline=None,
    ):
        class _FakeClient:
            def __init__(
                self,
                url,
                app_key,
                access_key,
                api_key,
                auth_method,
                config,
                ten_env=None,
                audio_timeline=None,
            ):
                self.url = url
                self.app_key = app_key
                self.access_key = access_key
                self.api_key = api_key
                self.auth_method = auth_method
                self.config = config
                self.ten_env = ten_env
                self.audio_timeline = audio_timeline
                self.connected = False
                self.on_result_callback = None
                self.on_error_callback = None
                self.on_connection_error_callback = None
                self.on_asr_error_callback = None
                self.on_connected_callback = None
                self.on_disconnected_callback = None
                self._emit_task = None

            def set_on_result_callback(self, callback):
                self.on_result_callback = callback

            def set_on_error_callback(self, callback):
                self.on_error_callback = callback

            def set_on_connection_error_callback(self, callback):
                self.on_connection_error_callback = callback

            def set_on_asr_error_callback(self, callback):
                self.on_asr_error_callback = callback

            def set_on_connected_callback(self, callback):
                self.on_connected_callback = callback

            def set_on_disconnected_callback(self, callback):
                self.on_disconnected_callback = callback

            async def connect(self):
                print("[mock] VolcengineASRClient.connect called")
                self.connected = True

                # Trigger connected callback
                if self.on_connected_callback:
                    self.on_connected_callback()

                # Schedule result emission
                async def _emit_results():
                    print("[mock] _emit_results task started")
                    try:
                        await asyncio.sleep(
                            0.5
                        )  # Give more time for audio to be sent
                    except asyncio.CancelledError:
                        print("[mock] _emit_results task cancelled")
                        return

                    # Emit interim result
                    if self.on_result_callback:
                        print("[mock] About to emit interim result")
                        try:
                            from ten_packages.extension.bytedance_llm_based_asr.volcengine_asr_client import (
                                ASRResponse,
                            )
                            from ten_packages.extension.bytedance_llm_based_asr.volcengine_asr_client import (
                                Utterance,
                            )

                            interim_result = ASRResponse()
                            interim_result.text = "hello"
                            interim_result.code = 0
                            interim_result.event = 1
                            interim_result.is_last_package = False
                            interim_result.payload_sequence = 1
                            interim_result.payload_size = 0
                            interim_result.payload_msg = {
                                "result": [
                                    {
                                        "text": "hello",
                                        "utterances": [
                                            {
                                                "text": "hello",
                                                "start_time": 0,
                                                "end_time": 1000,
                                                "definite": False,
                                            }
                                        ],
                                    }
                                ]
                            }
                            interim_result.result = {
                                "text": "hello",
                                "utterances": [
                                    {
                                        "text": "hello",
                                        "start_time": 0,
                                        "end_time": 1000,
                                        "definite": False,
                                    }
                                ],
                            }
                            interim_result.utterances = [
                                Utterance(
                                    text="hello",
                                    start_time=0,
                                    end_time=1000,
                                    definite=False,
                                )
                            ]
                            interim_result.start_ms = 0
                            interim_result.duration_ms = 1000
                            interim_result.language = "zh-CN"
                            interim_result.confidence = 0.9
                            print("[mock] emitting interim asr_result")
                            await self.on_result_callback(interim_result)
                            print("[mock] interim result emitted successfully")
                        except Exception as e:
                            print(f"[mock] Error emitting interim result: {e}")
                    else:
                        print(
                            "[mock] No on_result_callback set for interim result"
                        )

                    try:
                        await asyncio.sleep(0.5)
                    except asyncio.CancelledError:
                        print(
                            "[mock] _emit_results task cancelled during final result wait"
                        )
                        return

                    # Emit final result
                    if self.on_result_callback:
                        print("[mock] About to emit final result")
                        try:
                            final_result = ASRResponse()
                            final_result.text = "hello world"
                            final_result.code = 0
                            final_result.event = 1
                            final_result.is_last_package = True
                            final_result.payload_sequence = 2
                            final_result.payload_size = 0
                            final_result.payload_msg = {
                                "result": [
                                    {
                                        "text": "hello world",
                                        "utterances": [
                                            {
                                                "text": "hello world",
                                                "start_time": 0,
                                                "end_time": 2000,
                                                "definite": True,
                                            }
                                        ],
                                    }
                                ]
                            }
                            final_result.result = {
                                "text": "hello world",
                                "utterances": [
                                    {
                                        "text": "hello world",
                                        "start_time": 0,
                                        "end_time": 2000,
                                        "definite": True,
                                    }
                                ],
                            }
                            final_result.utterances = [
                                Utterance(
                                    text="hello world",
                                    start_time=0,
                                    end_time=2000,
                                    definite=True,
                                )
                            ]
                            final_result.start_ms = 0
                            final_result.duration_ms = 2000
                            final_result.language = "zh-CN"
                            final_result.confidence = 0.95
                            print("[mock] emitting final asr_result")
                            await self.on_result_callback(final_result)
                            print("[mock] final result emitted successfully")
                        except Exception as e:
                            print(f"[mock] Error emitting final result: {e}")
                    else:
                        print(
                            "[mock] No on_result_callback set for final result"
                        )

                print("[mock] Creating _emit_results task")
                self._emit_task = asyncio.create_task(_emit_results())
                print(f"[mock] Task created: {self._emit_task}")
                return None

            async def disconnect(self):
                print("[mock] VolcengineASRClient.disconnect called")
                self.connected = False

                # Cancel the emit task if it's running
                if self._emit_task and not self._emit_task.done():
                    print("[mock] Cancelling _emit_task")
                    self._emit_task.cancel()
                    try:
                        await self._emit_task
                    except asyncio.CancelledError:
                        print("[mock] _emit_task cancelled successfully")

                if self.on_disconnected_callback:
                    self.on_disconnected_callback()
                return None

            async def send_audio(self, audio_data):
                print(f"[mock] send_audio called with {len(audio_data)} bytes")
                return None

            async def listen(self):
                print("[mock] listen called")
                return None

        return _FakeClient(
            url, app_key, access_key, api_key, auth_method, config, ten_env
        )

    with patch(patch_target) as MockClient:
        MockClient.side_effect = _fake_ctor
        yield MockClient
