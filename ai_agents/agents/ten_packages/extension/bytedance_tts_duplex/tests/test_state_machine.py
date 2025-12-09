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
from bytedance_tts_duplex.bytedance_tts import (
    EVENT_TTSResponse,
    EVENT_TTSSentenceEnd,
    EVENT_SessionFinished,
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


@patch("bytedance_tts_duplex.extension.BytedanceV3Client")
def test_sequential_requests_state_machine(MockBytedanceV3Client):
    mock_instance = MockBytedanceV3Client.return_value
    mock_instance.finish_session = AsyncMock()
    mock_instance.finish_connection = AsyncMock()
    mock_instance.send_text = AsyncMock()
    mock_instance.close = AsyncMock()

    first_done = asyncio.Event()

    def ctor(
        config,
        ten_env,
        vendor,
        response_msgs,
        on_error=None,
        on_usage_characters=None,
        on_fatal_failure=None,
    ):
        mock_instance.response_msgs = response_msgs
        return mock_instance

    MockBytedanceV3Client.side_effect = ctor

    async def stream_first():
        await mock_instance.response_msgs.put((EVENT_TTSResponse, b"\x11\x22"))
        await mock_instance.response_msgs.put((EVENT_TTSSentenceEnd, {}))
        await mock_instance.response_msgs.put((EVENT_SessionFinished, b""))
        first_done.set()

    async def stream_second():
        await first_done.wait()
        await mock_instance.response_msgs.put((EVENT_TTSResponse, b"\x33\x44"))
        await mock_instance.response_msgs.put((EVENT_TTSSentenceEnd, {}))
        await mock_instance.response_msgs.put((EVENT_SessionFinished, b""))

    async def mock_send_text(text: str):
        if "First" in text:
            asyncio.create_task(stream_first())
        else:
            asyncio.create_task(stream_second())

    mock_instance.send_text.side_effect = mock_send_text

    tester = StateMachineExtensionTester()
    config = {"params": {"app_id": "a_valid_appid", "token": "a_valid_token"}}
    tester.set_test_mode_single("bytedance_tts_duplex", json.dumps(config))
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
