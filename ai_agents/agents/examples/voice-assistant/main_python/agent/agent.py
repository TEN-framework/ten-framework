import asyncio
import json
from .llm_exec import LLMExec
from ten_runtime import AsyncTenEnv, Cmd, CmdResult, Data, StatusCode
from ten_ai_base.types import LLMToolMetadata
from .events import *

class Agent:
    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env: AsyncTenEnv = ten_env
        self.stopped = False
        self.event_queue: asyncio.Queue[AgentEvent] = asyncio.Queue()
        self.llm_exec = LLMExec(ten_env)
        self.llm_exec.on_response = self._on_llm_response  # callback handled internally

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
                event = ToolRegisterEvent(tool=tool, source=cmd.get_source().extension_name)
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


    async def register_llm_tool(self, tool: LLMToolMetadata, source: str):
        """
        Register tools with the LLM.
        This method sends a command to register the provided tools.
        """
        await self.llm_exec.register_tool(tool, source)

    async def queue_llm_input(self, text: str):
        """
        Queue a new message to the LLM context.
        This method sends the text input to the LLM for processing.
        """
        await self.llm_exec.queue_input(text)

    async def flush_llm(self):
        """
        Flush the LLM input queue.
        This will ensure that all queued inputs are processed.
        """
        await self.llm_exec.flush()

    async def stop(self):
        """
        Stop the agent processing.
        This will stop the event queue and any ongoing tasks.
        """
        self.stopped = True
        await self.llm_exec.stop()
        await self.event_queue.put(None)

    async def _on_llm_response(self, ten_env: AsyncTenEnv, delta: str, text: str, is_final: bool):
        """
        Internal callback for streaming LLM output, wrapped as an AgentEvent.
        """
        event = LLMResponseEvent(
            delta=delta,
            text=text,
            is_final=is_final,
        )
        await self.event_queue.put(event)