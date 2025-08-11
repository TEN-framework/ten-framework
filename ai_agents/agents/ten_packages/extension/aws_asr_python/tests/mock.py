#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import pytest
import asyncio
from unittest.mock import MagicMock, patch

from amazon_transcribe.model import (
    TranscriptEvent,
    Transcript,
    Alternative,
    Item,
)

class MockInputStream:
    def __init__(self):
        self._input_stream = MagicMock()
        self._input_stream.closed = False
        self._input_stream.__done = False
        self.audio_chunks = []

    async def send_audio_event(self, audio_chunk: bytes):
        """simulate sending audio event"""
        self.audio_chunks.append(audio_chunk)
        # simulate IOError
        if len(self.audio_chunks) > 100:  # simulate stream closed
            self._input_stream.closed = True
            raise IOError("Stream closed")

    async def end_stream(self):
        """simulate end stream"""
        self._input_stream.closed = True
        self._input_stream.__done = True

class MockOutputStream:
    def __init__(self):
        self.words = [
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
        self.current_index = 0

    def __aiter__(self):
        return self

    async def __anext__(self):
        if self.current_index >= len(self.words):
            raise StopAsyncIteration
        
        word = self.words[self.current_index]
        self.current_index += 1
        
        # Create a real TranscriptEvent object
        # The last word is set to a non-partial result (final)
        is_partial = self.current_index < len(self.words)
        
        # Create Item
        item = Item(
            content=word,
            start_time=0.0,
            end_time=1.0,
            item_type="pronunciation",
            vocabulary_filter_match=False,
            stable=True,
        )
        
        # Create Alternative
        alternative = Alternative(
            transcript=" ".join(self.words[:self.current_index]),
            items=[item],
            entities=[]
        )
        
        # Create TranscriptResult
        result = MagicMock()
        result.result_id = "test_result"
        result.start_time = 0.0
        result.end_time = 1.0
        result.is_partial = is_partial
        result.alternatives = [alternative]
        
        # Create Transcript
        transcript = Transcript(results=[result])
        
        # Create TranscriptEvent
        event = TranscriptEvent(transcript=transcript)
        await asyncio.sleep(0.2)
        
        return event


class MockStream(object):
    def __init__(self, *args, **kwargs):
        super().__init__()
        self.output_stream = MockOutputStream()
        self.input_stream = MockInputStream()
    


class MockClient(object):
    def __init__(self, *args, **kwargs):
        super().__init__()
        self.stream = MockStream()

    
    async def start_stream_transcription(self, *args, **kwargs):
        return self.stream

@pytest.fixture(scope="function")
def patch_asr_client():
    with patch(
        "ten_packages.extension.aws_asr_python.extension.TranscribeStreamingClient"
    ) as MockTranscribeStreamingClient:
        MockTranscribeStreamingClient.side_effect = MockClient

        yield MockTranscribeStreamingClient
