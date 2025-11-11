from __future__ import annotations

import asyncio
import os

from ten_runtime import AsyncTenEnv


class MemosClient:
    """Simple client for MemOS memory management."""

    def __init__(self, env: AsyncTenEnv, api_key: str | None = None):
        self.env = env
        from memos.api.client import MemOSClient

        # Get API key from parameter or environment variable
        self.api_key = api_key or os.getenv("MEMOS_API_KEY", "")
        if not self.api_key:
            raise ValueError(
                "MemOS API key is required. Set MEMOS_API_KEY environment variable or provide api_key parameter."
            )

        self.client = MemOSClient(api_key=self.api_key)

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
            # Run synchronous client method in thread pool to avoid blocking
            await asyncio.to_thread(
                self.client.add_message, conversation, user_id, conversation_id
            )
            self.env.log_info(
                f"[MemosClient] Successfully added messages to MemOS"
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
            # Run synchronous client method in thread pool to avoid blocking
            memories = await asyncio.to_thread(
                self.client.search_memory, query, user_id, conversation_id
            )
            # Ensure we return a list of strings
            if not isinstance(memories, list):
                memories = []
            memories = [str(m) for m in memories if m]
            self.env.log_info(
                f"[MemosClient] Found {len(memories)} memories"
            )
            return memories
        except Exception as e:
            self.env.log_error(
                f"[MemosClient] Failed to search memories: {e}"
            )
            return []
