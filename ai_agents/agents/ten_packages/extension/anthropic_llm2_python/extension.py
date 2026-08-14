#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from typing import AsyncGenerator

from ten_ai_base.llm2 import AsyncLLM2BaseExtension
from ten_ai_base.struct import (
    LLMRequest,
    LLMRequestRetrievePrompt,
    LLMResponse,
    LLMResponseRetrievePrompt,
)
from ten_runtime.async_ten_env import AsyncTenEnv

from .anthropic_llm import AnthropicLLM, AnthropicLLM2Config

# How long a request that arrives during startup waits for on_start to
# finish before giving up. Building the HTTP client takes a few tens of
# milliseconds, and the runtime can deliver a cmd inside that window.
STARTUP_TIMEOUT_SECONDS = 10.0


class AnthropicLLM2Extension(AsyncLLM2BaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: AnthropicLLM2Config | None = None
        self.client: AnthropicLLM | None = None
        self._started = asyncio.Event()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_init")
        await super().on_init(ten_env)

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_start")
        await super().on_start(async_ten_env)

        try:
            # An exception escaping on_start reaches the runtime's
            # _exit_on_exception, which calls os._exit and takes the whole
            # worker down. One bad property -- an effort level that is not in
            # the Literal, say -- must fail this node, not the app.
            try:
                config_json, _ = await self.ten_env.get_property_to_json("")
                self.config = AnthropicLLM2Config.model_validate_json(
                    config_json
                )
            except Exception as err:
                async_ten_env.log_error(f"Invalid configuration: {err}")
                return

            if not self.config.api_key:
                async_ten_env.log_error("API key is missing, exiting on_start")
                return

            try:
                self.client = AnthropicLLM(async_ten_env, self.config)
                async_ten_env.log_info(
                    f"initialized with model: {self.config.model}, "
                    f"max_tokens: {self.config.max_tokens}, "
                    f"effort: {self.config.effort}"
                )
            except Exception as err:
                async_ten_env.log_error(
                    f"Failed to initialize AnthropicLLM: {err}"
                )
        finally:
            # Unblocks any chat_completion that raced startup, including the
            # failure paths above -- they must report the real reason rather
            # than time out.
            self._started.set()

    async def on_stop(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_stop")
        # Base class first, so in-flight streams are cancelled before their
        # connection pool goes away.
        await super().on_stop(async_ten_env)
        if self.client is not None:
            await self.client.aclose()
            self.client = None

    async def on_deinit(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_deinit")
        await super().on_deinit(async_ten_env)

    async def on_retrieve_prompt(
        self, async_ten_env: AsyncTenEnv, request: LLMRequestRetrievePrompt
    ) -> LLMResponseRetrievePrompt:
        """Retrieve the current prompt from config."""
        prompt = self.config.prompt if self.config else ""
        async_ten_env.log_info(
            f"Retrieved prompt for request_id: {request.request_id}"
        )
        return LLMResponseRetrievePrompt(prompt=prompt)

    def on_call_chat_completion(
        self, async_ten_env: AsyncTenEnv, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        return self._chat_completion(async_ten_env, request_input)

    async def _chat_completion(
        self, async_ten_env: AsyncTenEnv, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        # The runtime can deliver a cmd before on_start has built the client,
        # so wait for startup instead of failing a request that is merely
        # early.
        if not self._started.is_set():
            async_ten_env.log_info("Request arrived during startup; waiting")
            try:
                await asyncio.wait_for(
                    self._started.wait(), STARTUP_TIMEOUT_SECONDS
                )
            except asyncio.TimeoutError as err:
                raise RuntimeError(
                    "AnthropicLLM did not finish starting within "
                    f"{STARTUP_TIMEOUT_SECONDS}s"
                ) from err

        if self.client is None:
            raise RuntimeError(
                "AnthropicLLM is not initialized; check that api_key is set"
            )

        async for response in self.client.get_chat_completions(request_input):
            yield response
