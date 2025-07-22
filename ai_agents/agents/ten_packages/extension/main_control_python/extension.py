#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from enum import Enum
import json
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

class StateView:
    def __init__(self) -> None:
        self.last_asr_result: str = ""
        self.last_llm_result: str = ""
        self.rtc_user_count: int = 0

    async def handle_asr_result(self, asr_result: Data):
        text, _ = asr_result.get_property_to_json("text")
        self.last_asr_result = text

    async def handle_llm_result(self, llm_result: Data):
        text, _ = llm_result.get_property_to_json("text")
        self.last_llm_result = text

    async def handle_on_user_joined(self, on_user_joined: Cmd):
        self.rtc_user_count += 1

    async def handle_on_user_left(self, on_user_left: Cmd):
        self.rtc_user_count -= 1

    async def handle_flush(self):
        self.last_asr_result = ""
        self.last_llm_result = ""

class Controller:
    def __init__(self, ten_env: AsyncTenEnv, view: StateView) -> None:
        self._ten_env = ten_env
        self._view = view

    async def handle_asr_result(self, asr_result: Data):
        await self._view.handle_asr_result(asr_result)
        asr_result_json, _ = asr_result.get_property_to_json(None)
        asr_result_dict = json.loads(asr_result_json)
        text = asr_result_dict.get("text", "")
        final = asr_result_dict.get("final", False)
        metadata = asr_result_dict.get("metadata", {})
        stream_id = metadata.get("session_id", "100")
        if final or len(text) > 2:
            await self._flush()
        if final:
            await self._request_llm(text, True)

        await self._pass_message(text, final, final, stream_id)

    async def handle_llm_result(self, llm_result: Data):
        await self._view.handle_llm_result(llm_result)
        llm_result_json, _ = llm_result.get_property_to_json(None)
        llm_result_dict = json.loads(llm_result_json)
        text = llm_result_dict.get("text", "")
        end_of_segment = llm_result_dict.get("end_of_segment", False)
        await self._request_tts(text)
        await self._pass_message(text, end_of_segment, True, "100")

    async def handle_on_user_joined(self, on_user_joined: Cmd):
        await self._view.handle_on_user_joined(on_user_joined)
        if self._view.rtc_user_count == 1:
            await self._request_tts("Hello there, I'm TEN Agent")

    async def handle_on_user_left(self, on_user_left: Cmd):
        await self._view.handle_on_user_left(on_user_left)

    async def _request_llm(self, text: str, is_final: bool):
        q = Data.create("llm_request")
        q.set_property_string("text", text)
        q.set_property_bool("is_final", is_final)
        await self._ten_env.send_data(q)

    async def _request_tts(self, text: str):
        q = Data.create("tts_request")
        q.set_property_string("text", text)
        await self._ten_env.send_data(q)

    async def _flush(self):
        flush_llm = Cmd.create("flush")
        flush_llm.set_dest(None, None, "llm")
        await self._ten_env.send_cmd(flush_llm)

        flush_tts = Cmd.create("flush")
        flush_tts.set_dest(None, None, "tts")
        await self._ten_env.send_cmd(flush_tts)

        flush_rtc = Cmd.create("flush")
        flush_rtc.set_dest(None, None, "rtc")
        await self._ten_env.send_cmd(flush_rtc)

        await self._view.handle_flush()

    async def _pass_message(self, text: str, end_of_segment: bool, final: bool, stream_id: str):
        pass_message = Data.create("pass_message")
        pass_message.set_property_string("text", text)
        pass_message.set_property_bool("is_final", final)
        pass_message.set_property_string("stream_id", stream_id)
        pass_message.set_property_bool("end_of_segment", end_of_segment)
        await self._ten_env.send_data(pass_message)


class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")
        self._controller = Controller(ten_env, StateView())

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        if cmd_name == "on_user_joined":
            await self._on_cmd_on_user_joined(ten_env, cmd)
        elif cmd_name == "on_user_left":
            await self._on_cmd_on_user_left(ten_env, cmd)

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def _on_cmd_on_user_joined(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        await self._controller.handle_on_user_joined(cmd)

    async def _on_cmd_on_user_left(
        self, ten_env: AsyncTenEnv, cmd: Cmd
    ) -> None:
        await self._controller.handle_on_user_left(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info("on_data name {}".format(data_name))

        if data_name == "asr_result":
            await self._on_data_asr_result(ten_env, data)
        elif data_name == "llm_result":
            await self._on_data_llm_result(ten_env, data)

    async def _on_data_asr_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        await self._controller.handle_asr_result(data)

    async def _on_data_llm_result(
        self, ten_env: AsyncTenEnv, data: Data
    ) -> None:
        await self._controller.handle_llm_result(data)
