from __future__ import annotations

import asyncio
import json
from abc import ABC, abstractmethod
from typing import Any

from ten_runtime import AsyncTenEnv


class MemoryStore(ABC):
    def __init__(self, env: AsyncTenEnv):
        self.env = env

    @abstractmethod
    async def memorize(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None: ...

    @abstractmethod
    async def retrieve_default_categories(
        self, user_id: str, agent_id: str
    ) -> Any: ...

    @abstractmethod
    async def retrieve_related_clustered_categories(
        self, user_id: str, agent_id: str, category_query: str
    ) -> Any: ...

    @abstractmethod
    def parse_default_categories(self, data: Any) -> dict:
        """
        Normalize provider-specific response into a unified dict:
        {
          "basic_stats": {"total_categories": int, "total_memories": int, "user_id": str|None, "agent_id": str|None},
          "categories": [
            {"name": str, "type": str|None, "memory_count": int, "is_active": bool|None, "recent_memories": [{"date": str, "content": str}], "summary": str|None}
          ]
        }
        """
        ...

    @abstractmethod
    def parse_related_clustered_categories(self, data: Any) -> dict:
        """
        Normalize provider-specific response for related categories into a unified dict:
        {
          "query": str,
          "total_categories": int,
          "categories": [
            {
              "name": str,
              "summary": str|None,
              "description": str|None,
              "similarity_score": float|None,
              "memory_count": int,
              "recent_memories": [{"date": str, "content": str}]
            }
          ]
        }
        """
        ...


class PowerMemSdkMemoryStore(MemoryStore):
    def __init__(self, env: AsyncTenEnv):
        super().__init__(env)
        from powermem import Memory, auto_config

        self.client = Memory(config=auto_config())

    async def memorize(
        self,
        conversation: list[dict],
        user_id: str,
        agent_id: str,
    ) -> None:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] memorize called with user_id={user_id}, agent_id={agent_id}, "
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
            # Try to verify by querying immediately after (for debugging)
            try:
                verify_result = self.client.get_all(user_id=user_id)
                if isinstance(verify_result, dict):
                    verify_count = len(verify_result.get("results", []))
                elif isinstance(verify_result, list):
                    verify_count = len(verify_result)
                else:
                    verify_count = "unknown"
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Verification: get_all returned {verify_count} records for user_id='{user_id}'"
                )
            except Exception as verify_e:
                self.env.log_warn(
                    f"[PowerMemSdkMemoryStore] Could not verify saved data: {verify_e}"
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

    async def retrieve_default_categories(
        self, user_id: str, agent_id: str
    ) -> Any:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] retrieve_default_categories called with: "
            f"user_id='{user_id}', agent_id='{agent_id}'"
        )
        try:
            result = self.client.get_all(user_id=user_id)
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] retrieve_default_categories returned: {result}"
            )
            # Log detailed result structure
            if isinstance(result, dict):
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Result type: dict, keys: {list(result.keys())}"
                )
                if "results" in result:
                    self.env.log_info(
                        f"[PowerMemSdkMemoryStore] Results count: {len(result['results'])}"
                    )
                    for i, item in enumerate(result["results"][:3]):  # Log first 3 items
                        self.env.log_info(
                            f"[PowerMemSdkMemoryStore] Result item {i}: {item}"
                        )
            elif isinstance(result, list):
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Result type: list, length: {len(result)}"
                )
                for i, item in enumerate(result[:3]):  # Log first 3 items
                    self.env.log_info(
                        f"[PowerMemSdkMemoryStore] Result item {i}: {item}"
                    )
            else:
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Result type: {type(result)}"
                )
            return result
        except Exception as e:
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Failed to retrieve default categories: {e}"
            )
            import traceback
            self.env.log_error(
                f"[PowerMemSdkMemoryStore] Retrieve default categories traceback: {traceback.format_exc()}"
            )
            raise

    async def retrieve_related_clustered_categories(
        self, user_id: str, agent_id: str, category_query: str
    ) -> Any:
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories called with: "
            f"user_id='{user_id}', agent_id='{agent_id}', category_query='{category_query}'"
        )
        try:
            result = self.client.search(
                user_id=user_id, agent_id=agent_id, query=category_query
            )
            self.env.log_info(
                f"[PowerMemSdkMemoryStore] retrieve_related_clustered_categories returned: {result}"
            )
            # Log detailed result structure
            if isinstance(result, dict):
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Search result type: dict, keys: {list(result.keys())}"
                )
                results_count = len(result.get("results", []))
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Search found {results_count} results"
                )
                for i, item in enumerate(result.get("results", [])[:3]):  # Log first 3 items
                    self.env.log_info(
                        f"[PowerMemSdkMemoryStore] Search result item {i}: {item}"
                    )
            elif isinstance(result, list):
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Search result type: list, length: {len(result)}"
                )
                for i, item in enumerate(result[:3]):  # Log first 3 items
                    self.env.log_info(
                        f"[PowerMemSdkMemoryStore] Search result item {i}: {item}"
                    )
            else:
                self.env.log_info(
                    f"[PowerMemSdkMemoryStore] Search result type: {type(result)}"
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
        # PowerMem returns {"results": [...], "relations": [...] (optional)}
        # Add the query to the result so parse_related_clustered_categories can access it
        if isinstance(result, dict):
            result["query"] = category_query
        return result

    def parse_default_categories(self, data: Any) -> dict:
        """
        Parse PowerMem's get_all response format.
        PowerMem returns: {"results": [list of memory dicts]}
        Each memory dict has: {"id", "memory", "user_id", "agent_id", "metadata", "created_at", "updated_at"}
        """
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] parse_default_categories called with data type: {type(data)}"
        )
        # Extract results from PowerMem response
        if isinstance(data, dict):
            results = data.get("results", [])
        else:
            # Try to get results attribute if it's an object
            results = getattr(data, "results", [])

        if not results:
            return {
                "basic_stats": {
                    "total_categories": 0,
                    "total_memories": 0,
                    "user_id": None,
                    "agent_id": None,
                },
                "categories": [],
            }

        # Extract user_id and agent_id from first memory
        first_memory = results[0] if results else {}
        user_id = first_memory.get("user_id") if isinstance(
            first_memory, dict) else getattr(first_memory, "user_id", None)
        agent_id = first_memory.get("agent_id") if isinstance(
            first_memory, dict) else getattr(first_memory, "agent_id", None)

        # Group all memories into a single category (or create categories based on metadata if available)
        # For simplicity, we'll create one category with all memories
        recent_memories = []
        for memory in results:
            if isinstance(memory, dict):
                memory_content = memory.get("memory", "")
                created_at = memory.get("created_at")
            else:
                memory_content = getattr(memory, "memory", "")
                created_at = getattr(memory, "created_at", None)

            if not memory_content:
                continue

            # Format date
            date_str = None
            if created_at:
                try:
                    if isinstance(created_at, str):
                        # Try to parse ISO format string
                        from datetime import datetime
                        dt = datetime.fromisoformat(
                            created_at.replace('Z', '+00:00'))
                        date_str = dt.strftime("%Y-%m-%d %H:%M")
                    elif hasattr(created_at, "strftime"):
                        date_str = created_at.strftime("%Y-%m-%d %H:%M")
                    else:
                        date_str = str(created_at)
                except Exception:
                    date_str = str(created_at) if created_at else None

            recent_memories.append({
                "date": date_str,
                "content": memory_content
            })

        # Sort by date (most recent first)
        recent_memories.sort(key=lambda x: x["date"] or "", reverse=True)

        # Create a single category with all memories
        category = {
            "name": "All Memories",
            "type": None,
            "memory_count": len(recent_memories),
            "is_active": True,
            "recent_memories": recent_memories[:10],  # Limit to 10 most recent
            "summary": None,
        }

        result = {
            "basic_stats": {
                "total_categories": 1,
                "total_memories": len(recent_memories),
                "user_id": user_id,
                "agent_id": agent_id,
            },
            "categories": [category],
        }
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] parse_default_categories returned: {result['basic_stats']['total_categories']} category, {result['basic_stats']['total_memories']} memories"
        )
        return result

    def parse_related_clustered_categories(self, data: Any) -> dict:
        """
        Parse PowerMem's search response format.
        PowerMem returns: {"results": [list of result dicts], "relations": [...] (optional)}
        Each result dict has: {"memory", "metadata", "score", ...}
        """
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] parse_related_clustered_categories called with data type: {type(data)}"
        )
        # Extract results from PowerMem response
        if isinstance(data, dict):
            results = data.get("results", [])
            # PowerMem search doesn't return query, but we can try to get it
            query = data.get("query", "")
        else:
            results = getattr(data, "results", [])
            query = getattr(data, "query", "")

        if not results:
            return {
                "query": query,
                "total_categories": 0,
                "categories": [],
            }

        # Group results by similarity score ranges to create categories
        # Or treat each result as its own category-like item
        out_categories = []

        # Group results with similar scores together
        score_groups = {}
        for result in results:
            if isinstance(result, dict):
                score = result.get("score", 0.0)
                memory_content = result.get("memory", "")
                created_at = result.get("created_at")
            else:
                score = getattr(result, "score", 0.0)
                memory_content = getattr(result, "memory", "")
                created_at = getattr(result, "created_at", None)

            if not memory_content:
                continue

            # Create a category for each result (or group by score ranges)
            # For simplicity, we'll create one category per result, but group them by score ranges
            score_range = f"{int(score * 10) * 10}%"
            if score_range not in score_groups:
                score_groups[score_range] = []

            # Format date
            date_str = None
            if created_at:
                try:
                    if isinstance(created_at, str):
                        from datetime import datetime
                        dt = datetime.fromisoformat(
                            created_at.replace('Z', '+00:00'))
                        date_str = dt.strftime("%Y-%m-%d %H:%M")
                    elif hasattr(created_at, "strftime"):
                        date_str = created_at.strftime("%Y-%m-%d %H:%M")
                    else:
                        date_str = str(created_at)
                except Exception:
                    date_str = str(created_at) if created_at else None

            score_groups[score_range].append({
                "date": date_str,
                "content": memory_content,
                "score": score
            })

        # Create categories from score groups
        # Sort by numeric value of score_range (extract number from "XX%" string)
        # to ensure correct ordering (100% > 90% > 80% ...)
        for score_range, memories in sorted(
            score_groups.items(),
            # Extract numeric value from "XX%" string
            key=lambda x: int(x[0].rstrip("%")),
            reverse=True
        ):
            # Sort memories in this group by score (highest first)
            memories.sort(key=lambda x: x.get("score", 0), reverse=True)

            # Create category name from score range
            cat_name = f"Relevance: {score_range}"

            # Get the highest score in this group for similarity_score
            max_score = max((m.get("score", 0) for m in memories), default=0)

            cat_data = {
                "name": cat_name,
                "summary": None,
                "description": f"Memories with similarity scores in {score_range} range",
                "similarity_score": max_score,
                "memory_count": len(memories),
                "recent_memories": [
                    {"date": m.get("date"), "content": m.get("content")}
                    for m in memories[:5]  # Limit to 5 most relevant
                ],
            }
            out_categories.append(cat_data)

        result = {
            "query": query,
            "total_categories": len(out_categories),
            "categories": out_categories,
        }
        self.env.log_info(
            f"[PowerMemSdkMemoryStore] parse_related_clustered_categories returned: {result['total_categories']} categories for query='{query}'"
        )
        return result
