import asyncio
import json
from ten_runtime import AsyncTenEnv, Cmd, CmdResult, Data, StatusCode
from ten_ai_base.types import LLMToolMetadata
from .events import *

class Agent:
    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env: AsyncTenEnv = ten_env
        self.stopped = False
        self.event_queue: asyncio.Queue[AgentEvent] = asyncio.Queue()

    async def on_cmd(self, cmd: Cmd):
        cmd_name = cmd.get_name()
        try:
            if cmd_name == "on_user_joined":
                event = UserJoinedEvent()
            elif cmd_name == "on_user_left":
                event = UserLeftEvent()
            elif cmd_name == "tool_register":
                tool_json, err = cmd.get_property_to_json("tool")
                if err:
                    raise RuntimeError(f"Invalid tool metadata: {err}")
                tool = LLMToolMetadata.model_validate_json(tool_json)
                event = ToolRegisterEvent(tool=tool, source=cmd.get_source())
            else:
                self.ten_env.log_warn(f"Unhandled cmd: {cmd_name}")
                return

            await self.event_queue.put(event)
            await self.ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

        except Exception as e:
            self.ten_env.log_error(f"on_cmd error: {e}")
            await self.ten_env.return_result(CmdResult.create(StatusCode.ERROR, cmd))

    async def on_data(self, data: Data):
        data_name = data.get_name()
        try:
            if data_name == "asr_result":
                asr_json, _ = data.get_property_to_json(None)
                asr = json.loads(asr_json)
                event = ASRResultEvent(
                    text=asr.get("text", ""),
                    final=asr.get("final", False),
                    metadata=asr.get("metadata", {}),
                )
                await self.event_queue.put(event)
            else:
                self.ten_env.log_warn(f"Unhandled data: {data_name}")

        except Exception as e:
            self.ten_env.log_error(f"on_data error: {e}")

    async def get_event(self) -> AgentEvent:
        return await self.event_queue.get()
