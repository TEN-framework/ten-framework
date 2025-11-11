from __future__ import annotations

import asyncio
import json
import os

import requests

from ten_runtime import AsyncTenEnv


class MemosClient:
    """Simple client for MemOS memory management using HTTP requests."""

    def __init__(
        self, env: AsyncTenEnv, api_key: str | None = None, base_url: str | None = None
    ):
        self.env = env

        # Get API key from parameter or environment variable
        self.api_key = api_key or os.getenv("MEMOS_API_KEY", "")
        if not self.api_key:
            raise ValueError(
                "MemOS API key is required. Set MEMOS_API_KEY environment variable or provide api_key parameter."
            )

        # Get base URL from parameter or environment variable
        self.base_url = base_url or os.getenv("MEMOS_BASE_URL", "")
        if not self.base_url:
            raise ValueError(
                "MemOS base URL is required. Set MEMOS_BASE_URL environment variable or provide base_url parameter."
            )

        # Ensure base_url doesn't end with a slash
        self.base_url = self.base_url.rstrip("/")

    async def add_message(
        self,
        conversation: list[dict],
        user_id: str,
        conversation_id: str,
    ) -> None:
        """
        Store conversation messages in MemOS.

        Args:
            conversation: List of dicts with "role" and "content" keys
            user_id: User identifier
            conversation_id: Conversation identifier
        """
        try:
            self.env.log_info(
                f"[MemosClient] Adding messages: user_id={user_id}, conversation_id={conversation_id}, messages={len(conversation)}"
            )

            url = f"{self.base_url}/add/message"
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Token {self.api_key}",
            }
            data = {
                "user_id": user_id,
                "conversation_id": conversation_id,
                "messages": conversation,
            }

            # Run synchronous HTTP request in thread pool to avoid blocking
            def _make_request():
                response = requests.post(url=url, headers=headers, data=json.dumps(data))
                response.raise_for_status()
                return response.json()

            result = await asyncio.to_thread(_make_request)
            self.env.log_info(
                f"[MemosClient] Successfully added messages to MemOS: {result}"
            )
        except Exception as e:
            self.env.log_error(
                f"[MemosClient] Failed to add messages: {e}"
            )
            raise

    async def search_memory(
        self, query: str, user_id: str, conversation_id: str
    ) -> list[str]:
        """
        Search for memories using MemOS search_memory API.

        Args:
            query: Search query string (empty string for general memories)
            user_id: User identifier
            conversation_id: Conversation identifier

        Returns:
            List of memory strings
        """
        try:
            self.env.log_info(
                f"[MemosClient] Searching memories: query='{query}', user_id={user_id}, conversation_id={conversation_id}"
            )

            url = f"{self.base_url}/search/memory"
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Token {self.api_key}",
            }
            data = {
                "query": query,
                "user_id": user_id,
                "conversation_id": conversation_id,
            }

            # Run synchronous HTTP request in thread pool to avoid blocking
            def _make_request():
                response = requests.post(url=url, headers=headers, data=json.dumps(data))
                response.raise_for_status()
                return response.json()

            result = await asyncio.to_thread(_make_request)

            self.env.log_info(f"[MemosClient] Search memory result: {result}")

            # Parse response - extract memory strings from result
            # Response format: {"code": 0, "data": {"memory_detail_list": [...], ...}, "message": "ok"}
            # Based on MemOS documentation: https://memos-docs.openmem.net/cn/usecase/home_assistant
            memories = []
            if isinstance(result, dict) and result.get("code") == 0:
                data = result.get("data", {})
                if isinstance(data, dict):
                    memory_list = data.get("memory_detail_list", [])
                    if isinstance(memory_list, list):
                        # Extract memory_value from each memory item (matching SDK behavior)
                        for memory_item in memory_list:
                            if isinstance(memory_item, dict):
                                memory_value = memory_item.get("memory_value", "")
                                if memory_value:
                                    memories.append(str(memory_value))

            self.env.log_info(
                f"[MemosClient] Found {len(memories)} memories"
            )
            return memories
        except Exception as e:
            self.env.log_error(
                f"[MemosClient] Failed to search memories: {e}"
            )
            return []
