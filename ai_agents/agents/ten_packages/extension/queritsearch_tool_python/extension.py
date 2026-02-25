#
#
# Agora Real Time Engagement
# Created by Hu Yue Liu Or KKKPJSKEY in 2025-02.
# Copyright (c) 2024 Agora IO. All rights reserved.
#
#
import json
import aiohttp
from typing import Any, List
import traceback
import sys

from ten_runtime import (
    AsyncTenEnv,
    Data,
    Cmd,
)

from pydantic import BaseModel
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
)
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension

CMD_TOOL_REGISTER = "tool_register"
CMD_TOOL_CALL = "tool_call"
CMD_PROPERTY_NAME = "name"
CMD_PROPERTY_ARGS = "args"

TOOL_REGISTER_PROPERTY_NAME = "name"
TOOL_REGISTER_PROPERTY_DESCRIPTON = "description"
TOOL_REGISTER_PROPERTY_PARAMETERS = "parameters"
TOOL_CALLBACK = "callback"

TOOL_NAME = "querit_search"
TOOL_DESCRIPTION = "Use querit.ai to search for latest information. Call this function if you are not sure about the answer."
TOOL_PARAMETERS = {
    "type": "object",
    "properties": {
        "query": {
            "type": "string",
            "description": "The search query to call querit Search.",
        }
    },
    "required": ["query"],
}

PROPERTY_API_KEY = "api_key"  # Required

DEFAULT_QUERIT_SEARCH_ENDPOINT = "https://api.querit.ai/v1/search"


class QueritSearchToolConfig(BaseModel):
    api_key: str = ""


class QueritSearchToolExtension(AsyncLLMToolBaseExtension):

    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.session = None
        self.config = None
        self.k = 10

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")

        self.session = aiohttp.ClientSession()
        config_json, _ = await ten_env.get_property_to_json("")

        try:
            self.config = QueritSearchToolConfig.model_validate_json(
                config_json)
        except Exception as e:
            ten_env.log_error(f"Invalid Deepgram config: {e}")
            self.config = QueritSearchToolConfig.model_validate_json("{}")
            exc_type, exc_value, exc_tb = sys.exc_info()

            # 获取详细的异常堆栈信息
            exc_details = {
                "type": str(exc_type),
                "message": str(exc_value),
                "traceback": traceback.format_tb(exc_tb)
            }

            # 将异常信息转换为 JSON 格式
            error_data = Data.create("error")
            json_error = json.dumps(exc_details, indent=4)
            error_data.set_property_from_json(
                None,
                json_error,
            )
            await ten_env.send_data(error_data)
        ten_env.log_info(f"config: {self.config.api_key}")

        await super().on_init(ten_env)

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")
        if self.config.api_key:
            await super().on_start(ten_env)
        else:
            ten_env.log_info("API key is missing, exiting on_start")

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

        # clean up resources
        if self.session and not self.session.closed:
            await self.session.close()
            self.session = None  # Ensure it can't be reused accidentally

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        await super().on_cmd(ten_env, cmd)

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        ten_env.log_debug("get_tool_metadata")
        return [
            LLMToolMetadata(
                name=TOOL_NAME,
                description=TOOL_DESCRIPTION,
                parameters=[
                    LLMToolMetadataParameter(
                        name="query",
                        type="string",
                        description="The search query to call Querit Search.",
                        required=True,
                    ),
                ],
            )
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        if name == TOOL_NAME:
            result = await self._do_search(ten_env, args)
            # result = LLMCompletionContentItemText(text="I see something")
            return {"content": json.dumps(result),
                    "type": "llmresult"}

    async def _do_search(self, ten_env: AsyncTenEnv, args: dict) -> Any:
        if "query" not in args:
            raise ValueError("Failed to get property")

        query = args["query"]
        snippets = []
        results = await self._querit_search_results(ten_env, query, count=self.k)
        if len(results) == 0:
            return "No good Querit Search Result was found"

        for result in results:
            snippets.append(result["snippet"])

        return snippets

    async def _initialize_session(self, ten_env: AsyncTenEnv):
        if self.session is None or self.session.closed:
            ten_env.log_debug("Initializing new session")
            self.session = aiohttp.ClientSession()

    async def _querit_search_results(
        self, ten_env: AsyncTenEnv, search_term: str, count: int
    ) -> List[dict]:
        ten_env.log_debug("_querit_search_results count {}".format(count))
        await self._initialize_session(ten_env)
        headers = {
            'Accept': 'application/json',
            'Authorization': 'Bearer ' + self.config.api_key,
            'Content-Type': 'application/json',
        }
        payload = {
            "query": search_term
        }

        async with self.session as session:
            async with session.post(
                DEFAULT_QUERIT_SEARCH_ENDPOINT, headers=headers, json=payload
            ) as response:
                response.raise_for_status()
                search_results = await response.json()

        if "results" in search_results and "result" in search_results["results"]:
            return search_results["results"]["result"]
        return []
