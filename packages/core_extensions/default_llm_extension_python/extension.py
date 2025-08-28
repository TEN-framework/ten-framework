from typing_extensions import AsyncGenerator, override

from dataclasses import dataclass

from pydantic import BaseModel
from ten_ai_base.llm2 import AsyncLLM2BaseExtension
from ten_ai_base.struct import LLMRequest, LLMResponse
from ten_runtime import (
    AsyncTenEnv,
)


@dataclass
class DefaultLLMConfig(BaseModel):
    pass


class DefaultLLMExtension(AsyncLLM2BaseExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.config: DefaultLLMConfig | None = None

    @override
    async def on_call_chat_completion(
        self, ten_env: AsyncTenEnv, input: LLMRequest
    ) -> AsyncGenerator[LLMResponse, None]:
        raise NotImplementedError(
            "DefaultLLMExtension does not implement on_call_chat_completion"
        )
