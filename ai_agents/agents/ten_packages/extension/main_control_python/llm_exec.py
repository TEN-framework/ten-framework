#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import traceback
from typing import Awaitable, Callable, Optional
from ten_ai_base.helper import AsyncQueue
from ten_ai_base.struct import LLMOutput
from .helper import _send_cmd_ex, parse_sentences
from ten_runtime import AsyncTenEnv, StatusCode


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
            Callable[[AsyncTenEnv, str, bool], Awaitable[None]]
        ] = None
        self.sentence_fragment = ""
        self.current_task: Optional[asyncio.Task] = None
        self.loop = asyncio.get_event_loop()
        self.loop.create_task(self._process_input_queue())

    async def queue_input(self, item: str) -> None:
        await self.input_queue.put(item)

    async def flush(self) -> None:
        """
        Flush the input queue to ensure all items are processed.
        This is useful for ensuring that all pending inputs are handled before stopping.
        """
        await self.input_queue.flush()
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

    async def _process_input_queue(self):
        """
        Process the input queue for commands and data.
        This method runs in a loop, processing items from the queue.
        """
        while not self.stopped:
            try:
                item = await self.input_queue.get()
                self.current_task = self.loop.create_task(
                    self._send_to_llm(self.ten_env, item, is_final=False)
                )
                await self.current_task
            except asyncio.CancelledError:
                self.ten_env.log_info("LLMExec processing cancelled.")
            except Exception as e:
                self.ten_env.log_error(
                    f"Error processing input queue: {traceback.format_exc()}"
                )
            finally:
                self.current_task = None

    async def _send_to_llm(
        self, ten_env: AsyncTenEnv, text: str, is_final: bool = None
    ) -> None:
        response = _send_cmd_ex(
            ten_env,
            "chat_completion",
            "llm",
            {
                "messages": [{"role": "user", "content": text}],
                "streaming": True,
                "tools": [],
                "model": "qwen-max",
                "parameters": {
                    "max_tokens": 1000,
                    "temperature": 0.7,
                    "top_p": 1.0,
                    "frequency_penalty": 0.0,
                    "presence_penalty": 0.0,
                    "stop_sequences": [],
                },
            },
        )

        async for cmd_result, _ in response:
            if cmd_result and cmd_result.is_final() is False:
                ten_env.log_info(f"_send_to_llm: cmd_result {cmd_result}")
                if cmd_result.get_status_code() == StatusCode.OK:
                    response_json, _ = cmd_result.get_property_to_json(None)
                    ten_env.log_debug(
                        f"_send_to_llm: response_json {response_json}"
                    )
                    completion = LLMOutput.model_validate_json(response_json)
                    await self._handle_llm_response(completion, is_final=False)
        await self._handle_llm_response(None, is_final=True)

    async def _handle_llm_response(
        self, llm_output: LLMOutput | None, is_final: bool = False
    ):
        self.ten_env.log_info(f"_handle_llm_response: {llm_output}")

        if is_final:
            if self.on_response:
                await self.on_response(self.ten_env, "", True)
        else:
            text = llm_output.choice.delta.content
            if text:
                sentences, self.sentence_fragment = parse_sentences(
                    self.sentence_fragment, text
                )
                for sentence in sentences:
                    if self.on_response:
                        await self.on_response(self.ten_env, sentence, False)
