from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any

from ten_runtime import AsyncTenEnv


class MemoryStore(ABC):
    def __init__(self, env: AsyncTenEnv):
        self.env: AsyncTenEnv = env

    @abstractmethod
    async def add(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None: ...

    @abstractmethod
    async def search(
        self, user_id: str, agent_id: str, query: str
    ) -> Any: ...


class PowerMemSdkMemoryStore(MemoryStore):
    def __init__(self, env: AsyncTenEnv):
        super().__init__(env)
        from powermem import Memory, auto_config

        self.client: Memory = Memory(config=auto_config())

    async def add(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] add called with user_id={user_id}, agent_id={agent_id}, "
            f"conversation_length={len(conversation)}"
        )
        try:
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] Calling client.add with messages={conversation[:2]}... (showing first 2), "
                f"user_id='{user_id}', agent_id='{agent_id}'"
            )
            self.client.add(
                messages=conversation,
                user_id=user_id,
                agent_id=agent_id,
            )
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] Successfully added {len(conversation)} messages to memory"
            )
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to add messages to memory: {e}"
            )
            import traceback
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Memorize traceback: {traceback.format_exc()}"
            )
            raise

    async def search(
        self, user_id: str, agent_id: str, query: str
    ) -> Any:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories called with: "
            f"user_id='{user_id}', agent_id='{agent_id}', query='{query}'"
        )
        try:
            result = self.client.search(
                user_id=user_id, agent_id=agent_id, query=query
            )
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories returned: {result}"
            )
            return result
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to retrieve related clustered categories: {e}"
            )
            import traceback
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Retrieve related clustered categories traceback: {traceback.format_exc()}"
            )
            raise


class PowerMemSdkUserMemoryStore(MemoryStore):
    def __init__(self, env: AsyncTenEnv):
        super().__init__(env)
        from powermem import UserMemory, auto_config

        self.client: UserMemory = UserMemory(config=auto_config())

    async def add(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] add called with user_id={user_id}, agent_id={agent_id}, "
            f"conversation_length={len(conversation)}"
        )
        try:
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] Calling client.add with messages={conversation[:2]}... (showing first 2), "
                f"user_id='{user_id}', agent_id='{agent_id}'"
            )
            self.client.add(
                messages=conversation,
                user_id=user_id,
                agent_id=agent_id,
            )
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] Successfully added {len(conversation)} messages to memory"
            )
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to add messages to memory: {e}"
            )
            import traceback
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Memorize traceback: {traceback.format_exc()}"
            )
            raise

    async def search(
        self, user_id: str, agent_id: str, query: str
    ) -> Any:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories called with: "
            f"user_id='{user_id}', agent_id='{agent_id}', query='{query}'"
        )
        try:
            result = self.client.search(
                user_id=user_id, agent_id=agent_id, query=query
            )
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories returned: {result}"
            )
            return result
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to retrieve related clustered categories: {e}"
            )
            import traceback
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Retrieve related clustered categories traceback: {traceback.format_exc()}"
            )
            raise
