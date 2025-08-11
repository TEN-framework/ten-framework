import asyncio
import json
from ten_ai_base.const import CMD_PROPERTY_RESULT
from ten_ai_base.mllm import DATA_MLLM_IN_FUNCTION_CALL_OUTPUT, DATA_MLLM_IN_REGISTER_TOOL, DATA_MLLM_IN_SEND_MESSAGE_ITEM, DATA_MLLM_OUT_FUNCTION_CALL, DATA_MLLM_OUT_INTERRUPTED, DATA_MLLM_OUT_REQUEST_TRANSCRIPT, DATA_MLLM_OUT_RESPONSE_TRANSCRIPT, DATA_MLLM_OUT_SESSION_READY
from ten_ai_base.struct import MLLMClientFunctionCallOutput, MLLMClientRegisterTool, MLLMServerFunctionCall, MLLMServerInputTranscript, MLLMServerInterrupt, MLLMServerOutputTranscript, MLLMServerSessionReady
from ..helper import _send_cmd, _send_data
from ten_runtime import AsyncTenEnv, Cmd, CmdResult, Data, StatusCode
from ten_ai_base.types import LLMToolMetadata, LLMToolResult
from .events import *

class Agent:
    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env: AsyncTenEnv = ten_env
        self.stopped = False
        self.event_queue: asyncio.Queue[AgentEvent] = asyncio.Queue()
        self.tool_registry: dict[str, str] = {}

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
        self.ten_env.log_info(f"on_data: {data_name}")
        try:
            if data_name == DATA_MLLM_OUT_REQUEST_TRANSCRIPT:
                transcript_json, _ = data.get_property_to_json(None)
                transcript = MLLMServerInputTranscript.model_validate_json(transcript_json)
                event = InputTranscriptEvent(
                    delta=transcript.delta,
                    content=transcript.content,
                    metadata=transcript.metadata,
                    final=transcript.final,
                )
                await self.event_queue.put(event)
            elif data_name == DATA_MLLM_OUT_RESPONSE_TRANSCRIPT:
                response_json, _ = data.get_property_to_json(None)
                response = MLLMServerOutputTranscript.model_validate_json(response_json)
                event = OutputTranscriptEvent(
                    delta=response.delta or "",
                    content=response.content,
                    metadata=response.metadata,
                    is_final=response.final,
                )
                await self.event_queue.put(event)
            elif data_name == DATA_MLLM_OUT_SESSION_READY:
                session_json, _ = data.get_property_to_json(None)
                session = MLLMServerSessionReady.model_validate_json(session_json)
                event = SessionReadyEvent(metadata=session.metadata)
                await self.event_queue.put(event)
            elif data_name == DATA_MLLM_OUT_INTERRUPTED:
                interrupt_json, _ = data.get_property_to_json(None)
                interrupt = MLLMServerInterrupt.model_validate_json(interrupt_json)
                event = ServerInterruptEvent(metadata=interrupt.metadata)
                await self.event_queue.put(event)
            elif data_name == DATA_MLLM_OUT_FUNCTION_CALL:
                function_call_json, _ = data.get_property_to_json(None)
                function_call = MLLMServerFunctionCall.model_validate_json(function_call_json)
                event = FunctionCallEvent(
                    call_id=function_call.call_id,
                    function_name=function_call.name,
                    arguments=function_call.arguments,
                )
                await self.event_queue.put(event)
            else:
                self.ten_env.log_warn(f"Unhandled data: {data_name}")

        except Exception as e:
            self.ten_env.log_error(f"on_data error: {e}")

    async def get_event(self) -> AgentEvent:
        return await self.event_queue.get()

    async def register_tool(self, tool: LLMToolMetadata, source: str):
        """
        Register a tool with the agent.
        This method is typically called when a tool is registered by an extension.
        """
        self.ten_env.log_info(f"Registering tool: {tool.name} from {source}")
        self.tool_registry[tool.name] = source

        payload = MLLMClientRegisterTool(tool=tool).model_dump()
        await _send_data(
            self.ten_env,
            DATA_MLLM_IN_REGISTER_TOOL,
            "v2v",
            payload,
        )
        self.ten_env.log_info(f"[MainControlExtension] Registered tools: {tool.name} from {source}")

    async def call_tool(self, tool_call_id: str, name: str, arguments: str):
        """
        Handle a tool call event.
        This method is typically called when the MLLM server makes a function call.
        """
        self.ten_env.log_info(f"Handling tool call: {tool_call_id}, {name}, {arguments}")
        src_extension_name = self.tool_registry.get(name)
        result, _ = await _send_cmd(self.ten_env, "tool_call", src_extension_name, {
            "name": name,
            "arguments": json.loads(arguments)
        })

        if result.get_status_code() == StatusCode.OK:
            r, _ = result.get_property_to_json(
                CMD_PROPERTY_RESULT
            )
            tool_result: LLMToolResult = json.loads(r)

            self.ten_env.log_info(
                f"tool_result: {tool_result}"
            )

            if tool_result["type"] == "llmresult":
                result_content = tool_result["content"]
                if isinstance(result_content, str):
                    await _send_data(
                        self.ten_env,
                        DATA_MLLM_IN_FUNCTION_CALL_OUTPUT,
                        "v2v",
                        MLLMClientFunctionCallOutput(
                            output=result_content,
                            call_id=tool_call_id,
                        ).model_dump(),
                    )
                else:
                    self.ten_env.log_error(
                        f"Unknown tool result content: {result_content}"
                    )

    async def stop(self):
        """
        Stop the agent processing.
        This will stop the event queue and any ongoing tasks.
        """
        self.stopped = True
        # await self.llm_exec.stop()
        await self.event_queue.put(None)