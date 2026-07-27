#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
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


class AnthropicLLM2Extension(AsyncLLM2BaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: AnthropicLLM2Config | None = None
        self.client: AnthropicLLM | None = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_init")
        await super().on_init(ten_env)

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_start")
        await super().on_start(async_ten_env)

        config_json, _ = await self.ten_env.get_property_to_json("")
        self.config = AnthropicLLM2Config.model_validate_json(config_json)

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
            async_ten_env.log_error(f"Failed to initialize AnthropicLLM: {err}")

    async def on_stop(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_stop")
        await super().on_stop(async_ten_env)

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
        if self.client is None:
            raise RuntimeError(
                "AnthropicLLM is not initialized; check that api_key is set"
            )
        return self.client.get_chat_completions(request_input)
