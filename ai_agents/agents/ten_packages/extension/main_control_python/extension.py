#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from enum import Enum
import time
from typing import Any, Awaitable, Callable
import uuid
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from pydantic import BaseModel, Field


class Event(BaseModel):
    event_id: str
    name: str
    timestamp: int
    payload: dict[str, Any] = Field(default_factory=dict)
    metadata: dict[str, Any] = Field(default_factory=dict)


class EventStore:
    def __init__(self):
        self._events: list[Event] = []
        self._notify_queue: asyncio.Queue[Event] = asyncio.Queue()
        self._notify_task = asyncio.create_task(self._notify())
        self._listeners: dict[str, list[Callable[[Event], Awaitable[None]]]] = (
            {}
        )

    def register(self, name: str, listener: Callable[[Event], Awaitable[None]]):
        if name not in self._listeners:
            self._listeners[name] = []
        self._listeners[name].append(listener)

    async def pub_event(self, event: Event):
        self._events.append(event)
        await self._notify_queue.put(event)

    async def _notify(self):
        while True:
            event = await self._notify_queue.get()
            # TODO: potential bottleneck
            for listener in self._listeners[event.name]:
                await listener(event)

    async def pub_event_now(
        self, name: str, payload: dict[str, Any], metadata: dict[str, Any]
    ):
        await self.pub_event(
            Event(
                event_id=str(uuid.uuid4()),
                name=name,
                timestamp=int(time.time()),
                payload=payload,
                metadata=metadata,
            )
        )


# Input from asr
class AsrResult(BaseModel):
    id: str = ""
    text: str
    final: bool
    start_ms: int
    dureation_ms: int
    language: str
    # words
    metadata: dict[str, Any] = Field(default_factory=dict)


class LLMResult(BaseModel):
    text: str
    # end_of_segment


class AsrResultEvent(BaseModel):
    text: str
    final: str
    stream_id: str


class LLMResultEvent(BaseModel):
    text: str


class LLMRequestEvent(BaseModel):
    text: str


class TTSRequestEvent(BaseModel):
    text: str


class EventType(str, Enum):
    ASR_RESULT = "asr_result"
    LLM_REQUEST = "llm_request"
    LLM_RESULT = "llm_result"
    TTS_REQUEST = "tts_request"
    ON_USER_JOINED = "on_user_joined"
    ON_USER_LEFT = "on_user_left"
    FLUSH = "flush"


class RuleEngine:
    def __init__(self, registry: EventStore, view: "StateView") -> None:
        self._view = view
        self._registry = registry
        self._rules = []

        events = [
            EventType.ASR_RESULT,
            EventType.LLM_RESULT,
            EventType.ON_USER_JOINED,
        ]
        for event in events:
            registry.register(event, self._event_trigger)

        self._rules.append(self._rule_flush)
        self._rules.append(self._rule_request_llm)
        self._rules.append(self._rule_request_tts)
        self._rules.append(self._rule_greeting)

    async def run(self):
        # start tick trigger
        pass

    async def stop(self):
        # stop tick trigger
        # reject event trigger
        pass

    async def _event_trigger(self, event: Event):
        await self._eval_rules(event)

    async def _tick_trigger(self):
        pass

    async def _eval_rules(self, event: Event | None):
        for rule in self._rules:
            await rule(event)

    async def _rule_flush(self, event: Event | None):
        if event is None:
            return

        if event.name != EventType.ASR_RESULT:
            return

        asr_result_event = AsrResultEvent.model_validate(event.payload)

        if asr_result_event.final or len(asr_result_event.text) > 2:
            await self._registry.pub_event_now(EventType.FLUSH, {}, {})

    async def _rule_request_llm(self, event: Event | None):
        if event is None:
            return

        if event.name != EventType.ASR_RESULT:
            return

        asr_result_event = AsrResultEvent.model_validate(event.payload)

        if asr_result_event.final:
            await self._registry.pub_event_now(
                EventType.LLM_REQUEST, {"text": self._view.last_asr_result}, {}
            )

    async def _rule_request_tts(self, event: Event | None):
        if event is None:
            return

        if event.name != EventType.LLM_RESULT:
            return

        llm_result_event = LLMResultEvent.model_validate(event.payload)

        await self._registry.pub_event_now(
            EventType.TTS_REQUEST, {"text": self._view.last_llm_result}, {}
        )

    async def _rule_greeting(self, event: Event | None):
        if event is None:
            return

        if event.name != EventType.ON_USER_JOINED:
            return

        if self._view.rtc_user_count == 1:
            await self._registry.pub_event_now(
                EventType.TTS_REQUEST,
                {"text": "Hello there, I'm TEN Agent"},
                {},
            )


# TODO: build different views from rules
# TODO: support QueryView
class StateView:
    def __init__(self, registry: EventStore) -> None:
        self.last_asr_result: str = ""
        self.last_llm_result: str = ""
        self.rtc_user_count: int = 0

        registry.register(EventType.ASR_RESULT, self.watch_asr_result)
        registry.register(EventType.LLM_RESULT, self.watch_llm_result)
        registry.register(EventType.ON_USER_JOINED, self.watch_on_user_joined)
        registry.register(EventType.ON_USER_LEFT, self.watch_on_user_left)
        registry.register(EventType.FLUSH, self.watch_flush)

    async def watch_asr_result(self, event: Event):
        asr_result_event = AsrResultEvent.model_validate(event.payload)
        self.last_asr_result = asr_result_event.text

    async def watch_llm_result(self, event: Event):
        llm_result_event = LLMResult.model_validate(event.payload)
        self.last_llm_result = llm_result_event.text

    async def watch_on_user_joined(self, event: Event):
        self.rtc_user_count += 1

    async def watch_on_user_left(self, event: Event):
        self.rtc_user_count -= 1

    async def watch_flush(self, event: Event):
        self.last_asr_result = ""
        self.last_llm_result = ""


class Operator:
    def __init__(self, ten_env: AsyncTenEnv, registry: EventStore) -> None:
        self._ten_env = ten_env
        registry.register(EventType.LLM_REQUEST, self.request_llm)
        registry.register(EventType.TTS_REQUEST, self.request_tts)
        registry.register(EventType.FLUSH, self.flush)

    async def request_llm(self, event: Event):
        llm_request_event = LLMRequestEvent.model_validate(event.payload)
        q = Data.create("text_data")
        q.set_property_string("text", llm_request_event.text)
        await self._ten_env.send_data(q)

    async def request_tts(self, event: Event):
        tts_request_event = TTSRequestEvent.model_validate(event.payload)
        q = Data.create("tts_request")
        q.set_property_string("text", tts_request_event.text)
        await self._ten_env.send_data(q)

    async def flush(self, event: Event):
        flush_llm = Cmd.create("flush_llm")
        await self._ten_env.send_cmd(flush_llm)

        flush_tts = Cmd.create("flush_tts")
        await self._ten_env.send_cmd(flush_tts)

        flush_rtc = Cmd.create("flush_rtc")
        await self._ten_env.send_cmd(flush_rtc)


class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self._event_store = None
        self._state_view = None
        self._rule_engine = None
        self._operator = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self._event_store = EventStore()
        self._state_view = StateView(self._event_store)
        self._rule_engine = RuleEngine(self._event_store, self._state_view)
        self._operator = Operator(ten_env, self._event_store)
        ten_env.log_debug("MainControl on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("MainControl on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("MainControl on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("MainControl on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("MainControl on_cmd name {}".format(cmd_name))

        if cmd_name == "on_user_joined":
            await self._on_cmd_on_user_joined(ten_env, cmd)
        elif cmd_name == "on_user_left":
            await self._on_cmd_on_user_left(ten_env, cmd)

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def _on_cmd_on_user_joined(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        await self._event_store.pub_event_now(EventType.ON_USER_JOINED, {}, {})

    async def _on_cmd_on_user_left(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        await self._event_store.pub_event_now(EventType.ON_USER_LEFT, {}, {})

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info("MainControl on_data name {}".format(data_name))

        if data_name == "asr_result":
            await self._on_data_asr_result(ten_env, data)
        elif data_name == "llm_result":
            await self._on_data_llm_result(ten_env, data)

    async def _on_data_asr_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        json_str, _ = data.get_property_to_json(None)
        asr_result = AsrResult.model_validate_json(json_str)

        stream_id = int(asr_result.metadata.get("session_id", "100"))
        event = Event(
            event_id=asr_result.id,
            name="asr_result",
            timestamp=int(time.time()),
            payload={
                "text": asr_result.text,
                "final": asr_result.final,
                "stream_id": stream_id,
            },
        )
        await self._event_store.pub_event(event)

    async def _on_data_llm_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        json_str, _ = data.get_property_to_json(None)
        llm_result = LLMResult.model_validate_json(json_str)

        event = Event(
            event_id=str(uuid.uuid4()),
            name="llm_result",
            timestamp=int(time.time()),
            payload={"text": llm_result.text},
        )
        await self._event_store.pub_event(event)
