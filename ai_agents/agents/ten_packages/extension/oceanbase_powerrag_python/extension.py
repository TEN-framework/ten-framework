#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
#
from __future__ import annotations

import json
import time
import traceback
import uuid
from typing import AsyncGenerator, Optional

import aiohttp
from pydantic import BaseModel

from ten_runtime import AsyncTenEnv
from ten_ai_base.llm2 import AsyncLLM2BaseExtension
from ten_ai_base.struct import (
    LLMRequest,
    LLMRequestRetrievePrompt,
    LLMResponse,
    LLMResponseMessageDelta,
    LLMResponseMessageDone,
    LLMResponseRetrievePrompt,
)


# ------------------------------
# Config
# ------------------------------
class OceanBaseLLM2Config(BaseModel):
    base_url: str = ""
    api_key: str = ""
    ai_database_name: str = ""
    collection_id: str = ""
    user_id: str = "TenAgent"
    failure_info: str = ""


# ------------------------------
# Provider client
# ------------------------------
class OceanBaseChatClient:
    def __init__(self, ten_env: AsyncTenEnv, cfg: OceanBaseLLM2Config):
        self.ten_env = ten_env
        self.cfg = cfg
        self._session: Optional[aiohttp.ClientSession] = None

    async def _ensure_session(self) -> aiohttp.ClientSession:
        if self._session is None or self._session.closed:
            self._session = aiohttp.ClientSession()
        return self._session

    async def aclose(self) -> None:
        if self._session and not self._session.closed:
            await self._session.close()

    async def get_chat_completions(
        self, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        """
        Stream OceanBase PowerRAG answers as LLM2 messages.
        Emits LLMResponseMessageDelta chunks, then a single LLMResponseMessageDone.
        """
        # last user message wins
        prompt = ""
        for m in request_input.messages or []:
            if (m.role or "").lower() == "user":
                prompt = m.content or prompt

        response_id = str(uuid.uuid4())
        if not prompt:
            # still follow the delta/done contract
            now_ms = int(time.time() * 1000)
            yield LLMResponseMessageDelta(
                response_id=response_id,
                role="assistant",
                content="(no user message to send)",
                created=now_ms,
            )
            yield LLMResponseMessageDone(
                response_id=response_id, created=now_ms, role="assistant"
            )
            return

        session = await self._ensure_session()

        url = (
            f"{self.cfg.base_url}/"
            f"{self.cfg.ai_database_name}/collections/"
            f"{self.cfg.collection_id}/chat"
        )
        headers = {
            "Authorization": self.cfg.api_key,
            "Content-Type": "application/json",
        }
        payload = {"stream": True, "jsonFormat": True, "content": prompt}

        self.ten_env.log_info(f"[OceanBase] PUT {url}")
        self.ten_env.log_info(f"[OceanBase] payload: {json.dumps(payload)}")

        start_perf = time.perf_counter()

        try:
            async with session.put(url, json=payload, headers=headers) as resp:
                if resp.status != 200:
                    body = await resp.text()
                    msg = f"[OceanBase] HTTP {resp.status}: {body}"
                    self.ten_env.log_error(msg)
                    err_ms = int(time.time() * 1000)
                    # surface a friendly line, then Done
                    yield LLMResponseMessageDelta(
                        response_id=response_id,
                        role="assistant",
                        delta="PowerRAG request failed.",
                        content=self.cfg.failure_info
                        or "PowerRAG request failed.",
                        created=err_ms,
                    )
                    yield LLMResponseMessageDone(
                        response_id=response_id,
                        created=err_ms,
                        role="assistant",
                        content="",
                    )
                    return

                self.ten_env.log_info(
                    f"[OceanBase] connected in {time.perf_counter() - start_perf:.3f}s"
                )

                content = ""

                # stream SSE lines
                async for raw in resp.content:
                    if not raw:
                        continue
                    line = raw.decode("utf-8", errors="ignore").strip()
                    self.ten_env.log_info(f"[OceanBase] SSE line: {line}")
                    if not line or not line.startswith("data:"):
                        continue

                    data_str = line[5:].strip()
                    if not data_str:
                        continue

                    try:
                        data_json = json.loads(data_str)
                    except json.JSONDecodeError:
                        continue

                    answer = data_json.get("answer")
                    if not isinstance(answer, dict):
                        continue

                    chunk = answer.get("content")
                    if not chunk:
                        continue
                    if chunk == "[DONE]":
                        break

                    content += chunk
                    yield LLMResponseMessageDelta(
                        response_id=response_id,
                        role="assistant",
                        content=content,
                        delta=chunk,
                        created=int(time.time() * 1000),
                    )

        except Exception as e:
            traceback.print_exc()
            msg = f"[OceanBase] exception: {e}"
            self.ten_env.log_error(msg)
            err_ms = int(time.time() * 1000)
            yield LLMResponseMessageDelta(
                response_id=response_id,
                role="assistant",
                delta=self.cfg.failure_info or "PowerRAG request failed.",
                content=self.cfg.failure_info
                or "PowerRAG request failed with exception.",
                created=err_ms,
            )
            yield LLMResponseMessageDone(
                response_id=response_id,
                created=err_ms,
                role="assistant",
            )
            return

        # finalize
        yield LLMResponseMessageDone(
            response_id=response_id,
            created=int(time.time() * 1000),
            role="assistant",
        )


# ------------------------------
# Extension
# ------------------------------
class OceanBasePowerRAGExtension(AsyncLLM2BaseExtension):
    """
    OceanBase PowerRAG provider in LLM2 form, mirroring the DifyLLM2Extension pattern.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[OceanBaseLLM2Config] = None
        self.client: Optional[OceanBaseChatClient] = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_init")
        await super().on_init(ten_env)

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_start")
        await super().on_start(async_ten_env)

        cfg_json, _ = await self.ten_env.get_property_to_json("")
        self.config = OceanBaseLLM2Config.model_validate_json(cfg_json)

        missing = [
            key
            for key in (
                "base_url",
                "api_key",
                "ai_database_name",
                "collection_id",
            )
            if not getattr(self.config, key)
        ]
        if missing:
            async_ten_env.log_error(
                f"[OceanBase] missing config: {', '.join(missing)}"
            )
            return

        try:
            self.client = OceanBaseChatClient(async_ten_env, self.config)
            async_ten_env.log_info(
                f"[OceanBase] client ready: base_url={self.config.base_url}, "
                f"db={self.config.ai_database_name}, col={self.config.collection_id}"
            )
        except Exception as err:
            async_ten_env.log_error(f"[OceanBase] failed to init client: {err}")

    async def on_stop(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_stop")
        if self.client:
            await self.client.aclose()
        await super().on_stop(async_ten_env)

    async def on_deinit(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_deinit")
        await super().on_deinit(async_ten_env)

    async def on_retrieve_prompt(
        self, async_ten_env: AsyncTenEnv, request: LLMRequestRetrievePrompt
    ) -> LLMResponseRetrievePrompt:
        """Retrieve the current prompt. OceanBase PowerRAG doesn't use system prompts, so return empty."""
        async_ten_env.log_info(
            f"Retrieved prompt for request_id: {request.request_id}"
        )
        return LLMResponseRetrievePrompt(prompt="")

    def on_call_chat_completion(
        self, async_ten_env: AsyncTenEnv, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        return self.client.get_chat_completions(request_input)
