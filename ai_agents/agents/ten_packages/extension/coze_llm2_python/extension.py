# ------------------------------
# Extension (LLM2)
# ------------------------------
from typing import AsyncGenerator, Optional
from ten_ai_base.llm2 import AsyncLLM2BaseExtension
from ten_ai_base.struct import (
    LLMRequest,
    LLMRequestRetrievePrompt,
    LLMResponse,
    LLMResponseRetrievePrompt,
)
from .coze import CozeChatClient, CozeLLM2Config
from ten_runtime import AsyncTenEnv


class CozeLLM2Extension(AsyncLLM2BaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: Optional[CozeLLM2Config] = None
        self.client: Optional[CozeChatClient] = None

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_info("on_init")
        await super().on_init(ten_env)

    async def on_start(self, async_ten_env: AsyncTenEnv) -> None:
        async_ten_env.log_info("on_start")
        await super().on_start(async_ten_env)

        # Load config from extension properties (JSON)
        cfg_json, _ = await self.ten_env.get_property_to_json("")
        self.config = CozeLLM2Config.model_validate_json(cfg_json)

        if not self.config.bot_id or not self.config.token:
            async_ten_env.log_info("Missing bot_id or token, exiting on_start")
            return

        try:
            self.client = CozeChatClient(async_ten_env, self.config)
            async_ten_env.log_info(
                f"initialized Coze client: base_url={self.config.base_url}, bot_id={self.config.bot_id}"
            )
        except Exception as err:
            async_ten_env.log_info(
                f"Failed to initialize CozeChatClient: {err}"
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
        """Retrieve the current prompt. Coze doesn't use system prompts, so return empty."""
        async_ten_env.log_info(
            f"Retrieved prompt for request_id: {request.request_id}"
        )
        return LLMResponseRetrievePrompt(prompt="")

    def on_call_chat_completion(
        self, async_ten_env: AsyncTenEnv, request_input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        return self.client.get_chat_completions(request_input)
