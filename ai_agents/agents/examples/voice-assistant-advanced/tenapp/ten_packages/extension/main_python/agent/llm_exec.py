#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import traceback
from typing import Awaitable, Callable, Literal, Optional
from ten_ai_base.const import CMD_PROPERTY_RESULT
from ten_ai_base.helper import AsyncQueue
from ten_ai_base.struct import (
    LLMMessage,
    LLMMessageContent,
    LLMMessageFunctionCall,
    LLMMessageFunctionCallOutput,
    LLMRequest,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
    LLMResponseReasoningDelta,
    LLMResponseReasoningDone,
    LLMResponseToolCall,
    parse_llm_response,
)
from ten_ai_base.types import LLMToolMetadata, LLMToolResult
from ..helper import _send_cmd, _send_cmd_ex
from ten_runtime import AsyncTenEnv, Loc, StatusCode
import uuid


class LLMExec:
    """
    Context for LLM operations, including ASR and TTS.
    This class handles the interaction with the LLM, including processing commands and data.
    """

    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env
        self.input_queue = AsyncQueue()
        self.stopped = False
        self.on_response: Optional[
            Callable[[AsyncTenEnv, str, str, bool], Awaitable[None]]
        ] = None
        self.on_reasoning_response: Optional[
            Callable[[AsyncTenEnv, str, str, bool], Awaitable[None]]
        ] = None
        self.on_tool_call: Optional[
            Callable[[AsyncTenEnv, LLMToolMetadata], Awaitable[None]]
        ] = None
        self.current_task: Optional[asyncio.Task] = None
        self._input_processor_task: Optional[asyncio.Task] = None
        self.available_tools: list[LLMToolMetadata] = []
        self.tool_registry: dict[str, str] = {}
        self.available_tools_lock = (
            asyncio.Lock()
        )  # Lock to ensure thread-safe access
        self.contexts: list[LLMMessage] = []
        self.current_request_id: Optional[str] = None
        self.current_text = None
        # Track response_ids that have tool calls - content from these should be suppressed
        self._tool_call_response_ids: set[str] = set()
        self._pending_content: dict[str, str] = (
            {}
        )  # Buffer content by response_id

    def start(self) -> None:
        """Start the input processor task - call this after __init__"""
        if not self._input_processor_task:
            self._input_processor_task = asyncio.create_task(
                self._process_input_queue()
            )

    async def queue_input(self, item: str, role: str = "user") -> None:
        await self.input_queue.put((item, role))

    async def flush(self) -> None:
        """
        Flush the input queue to ensure all items are processed.
        This is useful for ensuring that all pending inputs are handled before stopping.
        """
        await self.input_queue.flush()
        if self.current_request_id:
            request_id = self.current_request_id
            self.current_request_id = None
            await _send_cmd(
                self.ten_env, "abort", "llm", {"request_id": request_id}
            )
        if self.current_task:
            self.current_task.cancel()

    async def stop(self) -> None:
        """
        Stop the LLMExec processing.
        This will stop the input queue processing and any ongoing tasks.
        """
        self.stopped = True
        await self.flush()
        if self.current_task:
            self.current_task.cancel()

    async def register_tool(self, tool: LLMToolMetadata, source: str) -> None:
        """
        Register tools with the LLM.
        This method sends a command to register the provided tools.
        """
        async with self.available_tools_lock:
            self.available_tools.append(tool)
            self.tool_registry[tool.name] = source

    async def _process_input_queue(self):
        """
        Process the input queue for commands and data.
        This method runs in a loop, processing items from the queue.
        """
        while not self.stopped:
            try:
                item = await self.input_queue.get()
                # Unpack tuple (text, role) - supports both old format (str) and new format (tuple)
                if isinstance(item, tuple):
                    text, role = item
                else:
                    text, role = item, "user"
                self.ten_env.log_info(
                    f"[LLMExec] Processing queued input (role={role}, len={len(text)} chars): '{text[:100]}...'"
                )
                new_message = LLMMessageContent(role=role, content=text)
                self.current_task = asyncio.create_task(
                    self._send_to_llm(self.ten_env, new_message)
                )
                await self.current_task
                self.ten_env.log_info(
                    "[LLMExec] Finished sending queued input to LLM"
                )
            except asyncio.CancelledError:
                self.ten_env.log_info("LLMExec processing cancelled.")
                text = self.current_text
                self.current_text = None
                if self.on_response and text:
                    await self.on_response(self.ten_env, "", text, True)
            except Exception as e:
                self.ten_env.log_error(
                    f"Error processing input queue: {traceback.format_exc()}"
                )
            finally:
                self.current_task = None

    async def _queue_context(
        self, ten_env: AsyncTenEnv, new_message: LLMMessage
    ) -> None:
        """
        Queue a new message to the LLM context.
        This method appends the new message to the existing context and sends it to the LLM.
        """
        ten_env.log_info(f"_queue_context: {new_message}")
        self.contexts.append(new_message)

    async def _write_context(
        self,
        ten_env: AsyncTenEnv,
        role: Literal["user", "assistant"],
        content: str,
    ) -> None:
        last_context = self.contexts[-1] if self.contexts else None
        if last_context and last_context.role == role:
            # If the last context has the same role, append to its content
            last_context.content = content
        else:
            # Otherwise, create a new context message
            new_message = LLMMessageContent(role=role, content=content)
            await self._queue_context(ten_env, new_message)

    async def _send_to_llm(
        self, ten_env: AsyncTenEnv, new_message: LLMMessage
    ) -> None:
        # Log EVERY call to _send_to_llm to track loop
        import traceback

        stack_trace = "".join(
            traceback.format_stack()[-4:-1]
        )  # Get last 3 frames
        ten_env.log_info(
            f"[LLM-SEND-CALLED] message_type={type(new_message).__name__} stack:\n{stack_trace}"
        )

        messages = self.contexts.copy()
        messages.append(new_message)
        request_id = str(uuid.uuid4())
        self.current_request_id = request_id
        llm_input = LLMRequest(
            request_id=request_id,
            messages=messages,
            model="qwen-max",
            streaming=True,
            parameters={"temperature": 0.7},
            tools=self.available_tools,
        )
        input_json = llm_input.model_dump()
        response = _send_cmd_ex(ten_env, "chat_completion", "llm", input_json)

        # Queue the new message to the context
        await self._queue_context(ten_env, new_message)

        async for cmd_result, _ in response:
            if cmd_result and cmd_result.is_final() is False:
                if cmd_result.get_status_code() == StatusCode.OK:
                    response_json, _ = cmd_result.get_property_to_json(None)
                    ten_env.log_info(
                        f"_send_to_llm: response_json {response_json}"
                    )
                    completion = parse_llm_response(response_json)
                    await self._handle_llm_response(completion)

    async def _handle_llm_response(self, llm_output: LLMResponse | None):
        self.ten_env.log_info(f"_handle_llm_response: {llm_output}")

        match llm_output:
            case LLMResponseMessageDelta():
                delta = llm_output.delta
                text = llm_output.content
                self.current_text = text
                if delta and self.on_response:
                    await self.on_response(self.ten_env, delta, text, False)
                if text:
                    await self._write_context(self.ten_env, "assistant", text)
            case LLMResponseMessageDone():
                text = llm_output.content
                self.current_text = None
                # Always send is_final=True to signal LLM completion,
                # even if text is empty (content may have been streamed via deltas)
                if self.on_response:
                    await self.on_response(self.ten_env, "", text or "", True)
            case LLMResponseReasoningDelta():
                delta = llm_output.delta
                text = llm_output.content
                if delta and self.on_reasoning_response:
                    await self.on_reasoning_response(
                        self.ten_env, delta, text, False
                    )
            case LLMResponseReasoningDone():
                text = llm_output.content
                if self.on_reasoning_response and text:
                    await self.on_reasoning_response(
                        self.ten_env, "", text, True
                    )
            case LLMResponseToolCall():
                self.ten_env.log_info(
                    f"_handle_llm_response: invoking tool call {llm_output.name}"
                )
                src_extension_name = self.tool_registry.get(llm_output.name)

                # Validate tool is registered
                if not src_extension_name:
                    self.ten_env.log_error(
                        f"Tool '{llm_output.name}' not registered in tool registry"
                    )
                    return

                try:
                    result, error = await _send_cmd(
                        self.ten_env,
                        "tool_call",
                        src_extension_name,
                        {
                            "name": llm_output.name,
                            "arguments": llm_output.arguments,
                        },
                    )

                    if error:
                        self.ten_env.log_error(
                            f"Tool call failed with error: {error}"
                        )
                        return
                except Exception as e:
                    self.ten_env.log_error(
                        f"Exception during tool call '{llm_output.name}': {e}"
                    )
                    return

                if result.get_status_code() == StatusCode.OK:
                    r, _ = result.get_property_to_json(CMD_PROPERTY_RESULT)
                    tool_result: LLMToolResult = json.loads(r)

                    self.ten_env.log_info(
                        f"[TOOL-RESULT-RECEIVED] tool={llm_output.name} result={tool_result}"
                    )

                    context_function_call = LLMMessageFunctionCall(
                        name=llm_output.name,
                        arguments=json.dumps(llm_output.arguments),
                        call_id=llm_output.tool_call_id,
                        id=llm_output.response_id,
                        type="function_call",
                    )
                    if tool_result["type"] == "llmresult":
                        result_content = tool_result["content"]
                        if isinstance(result_content, str):
                            await self._queue_context(
                                self.ten_env, context_function_call
                            )
                            await self._send_to_llm(
                                self.ten_env,
                                LLMMessageFunctionCallOutput(
                                    output=result_content,
                                    call_id=llm_output.tool_call_id,
                                    type="function_call_output",
                                ),
                            )
                        else:
                            self.ten_env.log_error(
                                f"Unknown tool result content: {result_content}"
                            )
                    elif tool_result["type"] == "requery":
                        # Requery type - not implemented yet
                        pass
                else:
                    self.ten_env.log_error("Tool call failed")
