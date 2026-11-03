#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from moss import MossClient, QueryOptions
from pydantic import BaseModel
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
)
from ten_runtime import AsyncTenEnv

TOOL_NAME = "search_knowledge_base"


class MossToolConfig(BaseModel):
    project_id: str = ""
    project_key: str = ""
    index_name: str = ""
    top_k: int = 3


class MossToolExtension(AsyncLLMToolBaseExtension):
    """Searches a Moss index held in memory, so a query makes no network call."""

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config = MossToolConfig()
        self.client: MossClient | None = None
        self.loading: asyncio.Task[bool] | None = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        config_json, _ = await ten_env.get_property_to_json(None)
        self.config = MossToolConfig.model_validate_json(config_json)
        self.loading = asyncio.create_task(self._load_index(ten_env))
        await super().on_start(ten_env)

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        if self.loading:
            self.loading.cancel()
            await asyncio.wait([self.loading])
        if self.client:
            await self.client.close()
        await super().on_stop(ten_env)

    async def _load_index(self, ten_env: AsyncTenEnv) -> bool:
        """Loads the index in the background. A search waits for it."""
        try:
            self.client = MossClient(
                self.config.project_id, self.config.project_key
            )
            await self.client.load_index(
                self.config.index_name, auto_refresh=True
            )
        except Exception as e:
            ten_env.log_error(f"Moss index not loaded, no results: {e}")
            return False
        return True

    async def _wait_for_index(self) -> bool:
        """Waits for the load. A load cancelled on stop counts as failed."""
        await asyncio.wait([self.loading])
        return not self.loading.cancelled() and self.loading.result()

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name=TOOL_NAME,
                description="Search the knowledge base for facts that answer the user's question.",
                parameters=[
                    LLMToolMetadataParameter(
                        name="query",
                        type="string",
                        description="The user's question or a focused search query.",
                        required=True,
                    )
                ],
            )
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        query = args.get("query", "")
        if name != TOOL_NAME or not query or not await self._wait_for_index():
            return {"type": "llmresult", "content": ""}
        try:
            result = await self.client.query(
                self.config.index_name,
                query,
                QueryOptions(top_k=self.config.top_k),
            )
        except Exception as e:
            ten_env.log_error(f"Moss search failed: {e}")
            return {"type": "llmresult", "content": ""}
        ten_env.log_info(
            f"Moss: {len(result.docs)} results in {result.time_taken_ms} ms"
        )
        return {
            "type": "llmresult",
            "content": "\n\n".join(doc.text for doc in result.docs),
        }
