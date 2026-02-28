#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from abc import ABC, abstractmethod
import asyncio
import json
import traceback
from typing import AsyncGenerator, Dict, Optional

from .struct import LLMRequest, LLMRequestAbort, LLMRequestRetrievePrompt, LLMResponse, LLMResponseRetrievePrompt
from ten_runtime import (
    AsyncExtension,
)
from ten_runtime.async_ten_env import AsyncTenEnv
from ten_runtime.cmd import Cmd
from ten_runtime.cmd_result import CmdResult, StatusCode


class AsyncLLM2BaseExtension(AsyncExtension, ABC):
    """
    Base class for implementing a Language Model Extension.
    This class provides a basic implementation for processing chat completions.
    It automatically handles the registration of tools and the processing of chat completions.
    Use queue_input_item to queue input items for processing.
    Use flush_input_items to flush the queue and cancel the current task.
    Override on_call_chat_completion and on_data_chat_completion to implement the chat completion logic.
    """

    # Create the queue for message processing

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None
        self._inflight: Dict[str, "AsyncLLM2BaseExtension._TaskCtx"] = {}
        self._lock = asyncio.Lock()


    async def on_init(self, async_ten_env: AsyncTenEnv) -> None:
        await super().on_init(async_ten_env)
        self.ten_env = async_ten_env

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        await super().on_start(async_ten_env)

    async def on_stop(self, async_ten_env: AsyncTenEnv) -> None:
        await self._cancel_all()
        await super().on_stop(async_ten_env)

    async def on_deinit(self, async_ten_env: AsyncTenEnv) -> None:
        await self._cancel_all()
        await super().on_deinit(async_ten_env)

    async def on_cmd(self, async_ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        async_ten_env.log_debug(f"[LLM2Base] on_cmd: {cmd_name}")
        try:
            if cmd_name == "chat_completion":
                payload, err = cmd.get_property_to_json(None)
                if err:
                    raise RuntimeError(f"Failed to get payload: {err}")

                req = LLMRequest.model_validate_json(payload)
                rid = req.request_id
                if not rid:
                    raise RuntimeError("LLMRequest.request_id is required")

                # Reject duplicates instead of replacing
                async with self._lock:
                    existing = self._inflight.get(rid)
                    if existing and not existing.task.done():
                        async_ten_env.log_info(
                            f"[LLM2Base] Duplicate request_id rejected: {rid}"
                        )
                        cr = CmdResult.create(StatusCode.ERROR, cmd)
                        cr.set_property_from_json(
                            None,
                            json.dumps({
                                "error": "request_id_already_running",
                                "message": "A chat_completion with this request_id is already in progress.",
                                "request_id": rid,
                            }),
                        )
                        await async_ten_env.return_result(cr)
                        return

                    # Start streaming task
                    await self._start_locked(async_ten_env, cmd, req)

                # Ack creation (streaming results will arrive from the task)
                # await async_ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

            elif cmd_name == "abort":
                payload, err = cmd.get_property_to_json(None)
                if err:
                    raise RuntimeError(f"Failed to get payload: {err}")

                abort = LLMRequestAbort.model_validate_json(payload)
                rid = abort.request_id

                if rid:
                    cancelled = await self._cancel_one(rid)
                    async_ten_env.log_info(
                        f"[LLM2Base] abort: request_id={rid}, cancelled={cancelled}"
                    )
                else:
                    await self._cancel_all()
                    async_ten_env.log_info("[LLM2Base] abort: all requests cancelled")

                await async_ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

            elif cmd_name == "retrieve_prompt":
                payload, err = cmd.get_property_to_json(None)
                if err:
                    raise RuntimeError(f"Failed to get payload: {err}")

                retrieve = LLMRequestRetrievePrompt.model_validate_json(payload)

                response = await self.on_retrieve_prompt(async_ten_env, retrieve)
                cmd_result = CmdResult.create(StatusCode.OK, cmd)
                cmd_result.set_property_from_json(None, response.model_dump_json())
                await async_ten_env.return_result(cmd_result)

            else:
                await async_ten_env.return_result(CmdResult.create(StatusCode.OK, cmd))

        except Exception:
            async_ten_env.log_error(f"[LLM2Base] on_cmd error:\n{traceback.format_exc()}")
            await async_ten_env.return_result(CmdResult.create(StatusCode.ERROR, cmd))

    # ---------------------------
    # Concurrency & task plumbing
    # ---------------------------

    class _TaskCtx:
        __slots__ = ("task", "cmd", "request_id")
        def __init__(self, task: asyncio.Task, cmd: Cmd, request_id: str):
            self.task = task
            self.cmd = cmd
            self.request_id = request_id

    async def _start_locked(self, ten_env: AsyncTenEnv, cmd: Cmd, req: LLMRequest) -> None:
        """Call with self._lock held. Starts a task and registers it in _inflight."""
        rid = req.request_id
        task = asyncio.create_task(self._run_stream(ten_env, cmd, req), name=f"llm2:{rid}")
        self._inflight[rid] = self._TaskCtx(task=task, cmd=cmd, request_id=rid)
        task.add_done_callback(lambda t, rid=rid: asyncio.create_task(self._cleanup_after(rid)))

    async def _run_stream(self, ten_env: AsyncTenEnv, cmd: Cmd, req: LLMRequest) -> None:
        rid = req.request_id
        try:
            gen = self.on_call_chat_completion(ten_env, req)
            async for chunk in gen:
                try:
                    cr = CmdResult.create(StatusCode.OK, cmd)
                    cr.set_property_from_json(None, chunk.model_dump_json())
                    cr.set_final(False)
                    await ten_env.return_result(cr)
                except Exception:
                    ten_env.log_error(
                        f"[LLM2Base] return_result streaming error (rid={rid}):\n{traceback.format_exc()}"
                    )

            final = CmdResult.create(StatusCode.OK, cmd)
            final.set_final(True)
            await ten_env.return_result(final)

        except asyncio.CancelledError:
            ten_env.log_info(f"[LLM2Base] stream cancelled (rid={rid})")
            try:
                final = CmdResult.create(StatusCode.OK, cmd)
                # Optionally attach abort metadata:
                # final.set_property_from_json(None, json.dumps({"aborted": True, "request_id": rid}))
                final.set_final(True)
                await ten_env.return_result(final)
            except Exception:
                ten_env.log_error(
                    f"[LLM2Base] error returning final for cancelled stream (rid={rid}):\n{traceback.format_exc()}"
                )
            raise
        except Exception:
            ten_env.log_error(f"[LLM2Base] stream error (rid={rid}):\n{traceback.format_exc()}")
            try:
                err_final = CmdResult.create(StatusCode.ERROR, cmd)
                err_final.set_final(True)
                await ten_env.return_result(err_final)
            except Exception:
                ten_env.log_error(
                    f"[LLM2Base] error returning ERROR final (rid={rid}):\n{traceback.format_exc()}"
                )

    async def _cleanup_after(self, request_id: str) -> None:
        async with self._lock:
            ctx = self._inflight.get(request_id)
            if ctx and ctx.task.done():
                self._inflight.pop(request_id, None)

    async def _cancel_one(self, request_id: str) -> bool:
        async with self._lock:
            ctx = self._inflight.get(request_id)
            if not ctx:
                return False
            if not ctx.task.done():
                ctx.task.cancel()
                return True
            return False

    async def _cancel_all(self) -> None:
        async with self._lock:
            for ctx in list(self._inflight.values()):
                if not ctx.task.done():
                    ctx.task.cancel()

    @abstractmethod
    async def on_retrieve_prompt(
        self, async_ten_env: AsyncTenEnv, request: LLMRequestRetrievePrompt
    ) -> LLMResponseRetrievePrompt:
        """Called when a prompt retrieval is requested."""
        raise NotImplementedError(
            "on_retrieve_prompt must be implemented in the subclass"
        )

    @abstractmethod
    def on_call_chat_completion(
        self, async_ten_env: AsyncTenEnv, input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        """Called when a chat completion is requested by cmd call. Implement this method to process the chat completion."""
        raise NotImplementedError(
            "on_call_chat_completion must be implemented in the subclass"
        )