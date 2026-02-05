from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Any, Dict

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
    async def search(self, user_id: str, agent_id: str, query: str) -> Any: ...

    @abstractmethod
    async def get_user_profile(self, user_id: str, agent_id: str) -> str: ...


class PowerMemSdkMemoryStore(MemoryStore):
    def __init__(self, config: Dict[str, Any], env: AsyncTenEnv):
        super().__init__(env)
        from powermem import Memory

        self.client: Memory = Memory(config=config)

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

    async def search(self, user_id: str, agent_id: str, query: str) -> Any:
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

    async def get_user_profile(self, user_id: str, agent_id: str) -> str:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] get_user_profile called with: "
            f"user_id='{user_id}', agent_id='{agent_id}'"
        )
        try:
            user_profile = await self.search(
                user_id=user_id,
                agent_id=agent_id,
                query="User Profile",
            )

            # Extract memory content from results using list comprehension
            results = user_profile.get("results", [])
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] get_user_profile found {len(results)} results"
            )
            memorise = [
                result["memory"]
                for result in results
                if isinstance(result, dict) and result.get("memory")
            ]

            # Format memory text using join for better performance
            if memorise:
                profile_content = (
                    "User Profile:\n"
                    + "\n".join(f"- {memory}" for memory in memorise)
                    + "\n"
                )
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] get_user_profile formatted {len(memorise)} memories into profile"
                )
            else:
                profile_content = ""
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] get_user_profile found no memories, returning empty profile"
                )

            return profile_content
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to get user profile: {e}"
            )
            import traceback

            self.env.log_error(
                f"[PowerMemSdkMemoryStore] get_user_profile traceback: {traceback.format_exc()}"
            )
            raise


class PowerMemSdkUserMemoryStore(MemoryStore):
    def __init__(self, config: Dict[str, Any], env: AsyncTenEnv):
        super().__init__(env)
        from powermem import UserMemory

        self.client: UserMemory = UserMemory(config=config)

    async def add(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None:
        self.env.log_info(
            f"[PowerMemSdkUserMemoryStore] add called with user_id={user_id}, agent_id={agent_id}, "
            f"conversation_length={len(conversation)}"
        )
        try:
            self.env.log_info(
                f"[PowerMemSdkUserMemoryStore] Calling client.add with messages={conversation[:2]}... (showing first 2), "
                f"user_id='{user_id}', agent_id='{agent_id}'"
            )
            self.client.add(
                messages=conversation,
                user_id=user_id,
                agent_id=agent_id,
            )
            self.env.log_info(
                f"[PowerMemSdkUserMemoryStore] Successfully added {len(conversation)} messages to memory"
            )
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Failed to add messages to memory: {e}"
            )
            import traceback

            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Memorize traceback: {traceback.format_exc()}"
            )
            raise

    async def search(self, user_id: str, agent_id: str, query: str) -> Any:
        self.env.log_info(
            f"[PowerMemSdkUserMemoryStore] retrieve_related_clustered_categories called with: "
            f"user_id='{user_id}', agent_id='{agent_id}', query='{query}'"
        )
        try:
            result = self.client.search(
                user_id=user_id, agent_id=agent_id, query=query
            )
            self.env.log_info(
                f"[PowerMemSdkUserMemoryStore] retrieve_related_clustered_categories returned: {result}"
            )
            return result
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Failed to retrieve related clustered categories: {e}"
            )
            import traceback

            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Retrieve related clustered categories traceback: {traceback.format_exc()}"
            )
            raise

    async def get_user_profile_by_query(
        self, user_id: str, agent_id: str
    ) -> str:
        self.env.log_info(
            f"[PowerMemSdkUserMemoryStore] get_user_profile_by_query called with: "
            f"user_id='{user_id}', agent_id='{agent_id}'"
        )
        try:
            user_profile = await self.search(
                user_id=user_id,
                agent_id=agent_id,
                query="User Profile",
            )

            # Extract memory content from results using list comprehension
            results = user_profile.get("results", [])
            self.env.log_info(
                f"[PowerMemSdkUserMemoryStore] get_user_profile_by_query found {len(results)} results"
            )
            memorise = [
                result["memory"]
                for result in results
                if isinstance(result, dict) and result.get("memory")
            ]

            # Format memory text using join for better performance
            if memorise:
                profile_content = (
                    "User Profile:\n"
                    + "\n".join(f"- {memory}" for memory in memorise)
                    + "\n"
                )
                self.env.log_info(
                    f"[PowerMemSdkUserMemoryStore] get_user_profile_by_query formatted {len(memorise)} memories into profile"
                )
            else:
                profile_content = ""
                self.env.log_info(
                    f"[PowerMemSdkUserMemoryStore] get_user_profile_by_query found no memories, returning empty profile"
                )

            return profile_content
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Failed to get user profile by query: {e}"
            )
            import traceback

            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] get_user_profile_by_query traceback: {traceback.format_exc()}"
            )
            raise

    async def get_user_profile(self, user_id: str, agent_id: str) -> str:
        self.env.log_info(
            f"[PowerMemSdkUserMemoryStore] get_user_profile called with: "
            f"user_id='{user_id}', agent_id='{agent_id}'"
        )
        try:
            profile = self.client.profile(
                user_id=user_id,
            )

            if profile is not None:
                profile_content = profile.get("profile_content", None)
                self.env.log_info(
                    f"[PowerMemSdkUserMemoryStore] get_user_profile retrieved profile from client.profile: "
                    f"profile_content={'present' if profile_content else 'None'}"
                )
            else:
                profile_content = None
                self.env.log_info(
                    f"[PowerMemSdkUserMemoryStore] get_user_profile client.profile returned None"
                )

            if profile_content is None:
                self.env.log_info(
                    f"[PowerMemSdkUserMemoryStore] get_user_profile profile_content is None, "
                    f"falling back to get_user_profile_by_query"
                )
                profile_content = await self.get_user_profile_by_query(
                    user_id=user_id,
                    agent_id=agent_id,
                )

            self.env.log_info(
                f"[PowerMemSdkUserMemoryStore] get_user_profile returning profile_content "
                f"(length={len(profile_content) if profile_content else 0})"
            )
            return profile_content
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] Failed to get user profile: {e}"
            )
            import traceback

            self.env.log_error(
                f"[PowerMemSdkUserMemoryStore] get_user_profile traceback: {traceback.format_exc()}"
            )
            raise
