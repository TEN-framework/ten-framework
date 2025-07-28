#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
from typing import Any, Optional, Tuple
from pydantic import BaseModel
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
    TenError,
)

class MainControlConfig(BaseModel):
    greeting: str = "Hello there, I'm TEN Agent"

class MainControlExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self._rtc_user_count = 0
        self.config = None
        self.cmd_events = asyncio.Queue[Tuple[str, Cmd]]()
        self.data_events = asyncio.Queue[Tuple[str, Data]]()
        self.ten_env: AsyncTenEnv = None
        self.stopped = False

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MainControlConfig.model_validate_json(config_json)
        ten_env.log_debug("on_init")

        asyncio.create_task(self._process_cmd_events())
        asyncio.create_task(self._process_data_events())

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
        self.stopped = True

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_info(f"on_cmd name {cmd_name}")

        await self.cmd_events.put([cmd_name, cmd])

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)


    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info(f"on_data name {data_name}")

        await self.data_events.put([data_name, data])

    async def _process_data_events(self) -> None:
        while self.stopped is False:
            try:
                data_name, data = await self.data_events.get()
                match data_name:
                    case "asr_result":
                        asr_result_json, _ = data.get_property_to_json(None)
                        asr_result_dict = json.loads(asr_result_json)
                        text = asr_result_dict.get("text", "")
                        final = asr_result_dict.get("final", False)
                        metadata = asr_result_dict.get("metadata", {})
                        stream_id = int(metadata.get("session_id", "100"))
                        if final or len(text) > 2:
                            await self._interrupt(self.ten_env)
                        if final:
                            await self._send_to_llm(self.ten_env, text, True)

                        await self._send_transcript(self.ten_env, text, final, final, stream_id)
                    case "llm_result":
                        llm_result_json, _ = data.get_property_to_json(None)
                        llm_result_dict = json.loads(llm_result_json)
                        text = llm_result_dict.get("text", "")
                        end_of_segment = llm_result_dict.get("end_of_segment", False)
                        await self._send_to_tts(self.ten_env, text)
                        await self._send_transcript(self.ten_env, text, end_of_segment, True, 100)
                    case _:
                        self.ten_env.log_info(f"Unknown data: {data_name}")
            except Exception as e:
                self.ten_env.log_error(f"Error processing data: {e}")

    async def _process_cmd_events(self) -> None:
        while self.stopped is False:
            try:
                cmd_name, cmd = await self.cmd_events.get()

                match cmd_name:
                    case "on_user_joined":
                        self._rtc_user_count += 1
                        if self._rtc_user_count == 1 and self.config.greeting:
                            await self._send_to_tts(self.ten_env, self.config.greeting)
                            await self._send_transcript(self.ten_env, self.config.greeting, True, True, 100)
                    case "on_user_left":
                        self._rtc_user_count -= 1
                    case _:
                        self.ten_env.log_info(f"Unknown command: {cmd_name}")
            except Exception as e:
                self.ten_env.log_error(f"Error processing command: {e}")


    async def _send_to_tts(self, ten_env: AsyncTenEnv, text: str):
        await self._send_data(ten_env, "text_data", "tts", {"text": text})
        ten_env.log_info(f"_send_to_tts: text {text}")

    async def _send_to_llm(self, ten_env: AsyncTenEnv, text: str, is_final: bool):
        await self._send_data(ten_env, "text_data", "llm", {"text": text, "is_final": is_final})
        ten_env.log_info(f"_send_to_llm: text {text}, is_final {is_final}")

    async def _send_transcript(self, ten_env: AsyncTenEnv, text: str, end_of_segment: bool, final: bool, stream_id: int):
        await self._send_data(ten_env, "text_data", "message_collector", {"text": text, "is_final": final, "stream_id": stream_id, "end_of_segment": end_of_segment})
        ten_env.log_info(f"_send_transcript: text {text}, is_final {final}, end_of_segment {end_of_segment}, stream_id {stream_id}")

    async def _interrupt(self, ten_env: AsyncTenEnv):
        await self._send_cmd(ten_env, "flush", "llm")
        await self._send_cmd(ten_env, "flush", "tts")
        await self._send_cmd(ten_env, "flush", "rtc")


    async def _send_cmd(
        self, ten_env: AsyncTenEnv, cmd_name: str, dest: str, payload: Any = None
    ) -> tuple[Optional[CmdResult], Optional[TenError]]:
        """Convenient method to send a command with a payload within app/graph w/o need to create a connection."""
        """Note: extension using this approach will contain logics that are meaningful for this graph only,"""
        """as it will assume the target extension already exists in the graph."""
        """For generate purpose extension, it should try to prevent using this method."""
        cmd = Cmd.create(cmd_name)
        cmd.set_dest(None, None, dest)
        if payload is not None:
            cmd.set_property_from_json(None, json.dumps(payload))
        ten_env.log_debug(f"send_cmd: cmd_name {cmd_name}, dest {dest}")
        return await ten_env.send_cmd(cmd)

    async def _send_data(
        self, ten_env: AsyncTenEnv, data_name: str, dest: str, payload: Any = None
    ) -> Optional[TenError]:
        """Convenient method to send data with a payload within app/graph w/o need to create a connection."""
        """Note: extension using this approach will contain logics that are meaningful for this graph only,"""
        """as it will assume the target extension already exists in the graph."""
        """For generate purpose extension, it should try to prevent using this method."""
        data = Data.create(data_name)
        data.set_dest(None, None, dest)
        if payload is not None:
            data.set_property_from_json(None, json.dumps(payload))
        ten_env.log_debug(f"send_data: data_name {data_name}, dest {dest}")
        return await ten_env.send_data(data)