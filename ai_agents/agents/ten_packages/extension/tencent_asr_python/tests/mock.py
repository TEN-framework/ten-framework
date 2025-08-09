#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import pytest
import asyncio
import uuid
from typing import Callable
from unittest.mock import MagicMock, patch, AsyncMock
from tencent_asr_client import ResponseData, RecoginizeResult, AsyncTencentAsrListener


class MockClient(object):
    def __init__(self, *args, **kwargs):
        super().__init__()
        self.listener: AsyncTencentAsrListener = kwargs["listener"]
        self.kwargs = kwargs
        self._is_connected = False
        self.mock_response_callback: Callable = self._mock_response
        # self.send_pcm_data = AsyncMock()
        assert self.listener is not None, "listener is required"

    async def _mock_response(self, voice_id: str | None = None):
        await asyncio.sleep(1)

        words = [
            "",
            "hello",
            "world",
            "I'm",
            "the",
            "ten",
            "framework",
            "extension",
            "test",
            "case",
        ]
        for index, word in enumerate(words):
            if index == 0:
                slice_type = RecoginizeResult.SliceType.START
                fun = self.listener.on_asr_sentence_start
            elif index == len(words) - 1:
                slice_type = RecoginizeResult.SliceType.END
                fun = self.listener.on_asr_sentence_end
            else:
                slice_type = RecoginizeResult.SliceType.PROCESSING
                fun = self.listener.on_asr_sentence_change

            voice_text_str = " ".join(words[: index + 1])
            await fun(
                ResponseData[RecoginizeResult](
                    code=0,
                    message="success",
                    voice_id=voice_id,
                    result=RecoginizeResult(
                        slice_type=slice_type,
                        index=index,
                        start_time=0,
                        end_time=0,
                        voice_text_str=voice_text_str,
                        word_size=0,
                        word_list=[],
                        emotion_type=None,
                        speaker_info=None,
                    ),
                )
            )
            await asyncio.sleep(0.2)

        await self.listener.on_asr_complete(
            ResponseData(
                code=0,
                message="success",
                voice_id=voice_id,
                final=True,
                result=RecoginizeResult(
                    slice_type=0,
                    index=0,
                    start_time=0,
                    end_time=0,
                    voice_text_str="",
                    word_size=0,
                    word_list=[],
                    emotion_type=None,
                    speaker_info=None,
                ),
            )
        )

    async def send_pcm_data(self, data: bytes):
        pass

    async def send_end_of_stream(self):
        pass

    async def send_heartbeat(self):
        pass

    async def start(self):
        self._is_connected = True
        voice_id = str(uuid.uuid4())
        await self.listener.on_asr_start(
            ResponseData(code=0, message="success", voice_id=voice_id)
        )
        if self.mock_response_callback is not None:
            await self.mock_response_callback(voice_id)

    async def stop(self):
        self._is_connected = False

    async def send(self, data: bytes):
        pass

    def is_connected(self):
        return self._is_connected


@pytest.fixture(scope="function")
def patch_tencent_asr_client():
    with patch(
        "ten_packages.extension.tencent_asr_python.extension.TencentAsrClient"
    ) as MockTencentAsrClient:
        MockTencentAsrClient.side_effect = MockClient
        yield MockTencentAsrClient
