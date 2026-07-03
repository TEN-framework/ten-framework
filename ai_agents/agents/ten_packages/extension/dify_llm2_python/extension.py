#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from typing import AsyncGenerator, Optional

from ten_ai_base.llm2 import AsyncLLM2BaseExtension
from ten_ai_base.struct import (
    LLMRequest,
    LLMRequestRetrievePrompt,
    LLMResponse,
    LLMResponseRetrievePrompt,
)
from .dify import DifyChatClient, DifyLLM2Config
from ten_runtime import (
    AsyncTenEnv,
)


class DifyLLM2Extension(AsyncLLM2BaseExtension):
    """
    Drop-in provider that mirrors OpenAILLM2Extension structure:
    - loads config on start
    - forwards on_call_chat_completion to client.get_chat_completions
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[DifyLLM2Config] = None
        self.client: Optional[DifyChatClient] = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_init")
        await super().on_init(ten_env)

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_start")
        await super().on_start(async_ten_env)

        # Load config
        config_json, _ = await self.ten_env.get_property_to_json("")
        self.config = DifyLLM2Config.model_validate_json(config_json)
        if not self.config.api_key:
            async_ten_env.log_info("API key is missing, exiting on_start")
            return

        # Create client
        try:
            self.client = DifyChatClient(async_ten_env, self.config)
            async_ten_env.log_info(
                f"initialized Dify client: base_url={self.config.base_url}, user_id={self.config.user_id}"
            )
        except Exception as err:
            async_ten_env.log_info(
                f"Failed to initialize DifyChatClient: {err}"
            )

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
        """Retrieve the current prompt. Dify doesn't use system prompts, so return empty."""
        async_ten_env.log_info(
            f"Retrieved prompt for request_id: {request.request_id}"
        )
        return LLMResponseRetrievePrompt(prompt="")

    def on_call_chat_completion(
        self, async_ten_env: AsyncTenEnv, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        # Delegate to provider client (matches OpenAILLM2Extension)
        return self.client.get_chat_completions(request_input)
