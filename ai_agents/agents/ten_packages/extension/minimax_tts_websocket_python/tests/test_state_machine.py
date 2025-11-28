import sys
from pathlib import Path

project_root = str(Path(__file__).resolve().parents[6])
if project_root not in sys.path:
    sys.path.insert(0, project_root)

import asyncio
import json
from unittest.mock import patch, AsyncMock

from ten_runtime import ExtensionTester, TenEnvTester, Data
from ten_ai_base.struct import TTSTextInput
from minimax_tts_websocket_python.minimax_tts import (
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
)


class StateMachineExtensionTester(ExtensionTester):
    def __init__(self):
        super().__init__()
        self.audio_start_events = []
        self.audio_end_events = []
        self.request1_id = "state_test_req_1"
        self.request2_id = "state_test_req_2"
        self.test_completed = False

    def on_start(self, ten_env_tester: TenEnvTester) -> None:
        t1 = TTSTextInput(
            request_id=self.request1_id,
            text="First request text",
            text_input_end=True,
        )
        d1 = Data.create("tts_text_input")
        d1.set_property_from_json(None, t1.model_dump_json())
        ten_env_tester.send_data(d1)

        t2 = TTSTextInput(
            request_id=self.request2_id,
            text="Second request text",
            text_input_end=True,
        )
        d2 = Data.create("tts_text_input")
        d2.set_property_from_json(None, t2.model_dump_json())
        ten_env_tester.send_data(d2)

        ten_env_tester.on_start_done()

    def on_data(self, ten_env: TenEnvTester, data: Data) -> None:
        name = data.get_name()
        if name == "tts_audio_start":
            payload, _ = data.get_property_to_json("")
            payload_dict = (
                eval(payload) if isinstance(payload, str) else payload
            )
            rid = payload_dict.get("request_id", "")
            self.audio_start_events.append(rid)
        elif name == "tts_audio_end":
            payload, _ = data.get_property_to_json("")
            payload_dict = (
                eval(payload) if isinstance(payload, str) else payload
            )
            rid = payload_dict.get("request_id", "")
            reason = payload_dict.get("reason", 0)
            self.audio_end_events.append((rid, reason))
            if len(self.audio_end_events) == 2:
                self.test_completed = True
                ten_env.stop_test()


@patch("minimax_tts_websocket_python.extension.MinimaxTTSWebsocket")
def test_sequential_requests_state_machine(MockMinimaxTTSWebsocket):
    mock_instance = MockMinimaxTTSWebsocket.return_value
    mock_instance.start = AsyncMock()
    mock_instance.stop = AsyncMock()

    def ctor(
        config,
        ten_env,
        vendor,
        on_transcription=None,
        on_error=None,
        on_audio_data=None,
        on_usage_characters=None,
    ):
        mock_instance.on_audio_data = on_audio_data
        return mock_instance

    MockMinimaxTTSWebsocket.side_effect = ctor

    first_done = asyncio.Event()

    async def stream_first():
        await mock_instance.on_audio_data(b"\x01\x02", EVENT_TTSResponse, 0)
        await mock_instance.on_audio_data(None, EVENT_TTSSentenceEnd, 0)
        first_done.set()

    async def stream_second():
        await first_done.wait()
        await mock_instance.on_audio_data(b"\x03\x04", EVENT_TTSResponse, 0)
        await mock_instance.on_audio_data(None, EVENT_TTSSentenceEnd, 0)

    async def mock_get(tts_input):
        if "First" in tts_input.text:
            asyncio.create_task(stream_first())
        else:
            asyncio.create_task(stream_second())

    mock_instance.get.side_effect = mock_get

    tester = StateMachineExtensionTester()
    config = {"params": {"key": "test_key", "group_id": "test_group"}}
    tester.set_test_mode_single(
        "minimax_tts_websocket_python", json.dumps(config)
    )
    tester.run()

    assert tester.test_completed
    assert len(tester.audio_start_events) == 2
    assert len(tester.audio_end_events) == 2
    assert tester.audio_end_events[0][1] == 1
    assert tester.audio_end_events[1][1] == 1
    i1 = tester.audio_start_events.index(tester.request1_id)
    i2 = tester.audio_start_events.index(tester.request2_id)
    assert i1 < i2
    e1 = next(
        i
        for i, e in enumerate(tester.audio_end_events)
        if e[0] == tester.request1_id
    )
    e2 = next(
        i
        for i, e in enumerate(tester.audio_end_events)
        if e[0] == tester.request2_id
    )
    assert e1 < e2
