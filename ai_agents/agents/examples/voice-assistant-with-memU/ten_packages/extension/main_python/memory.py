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
        user_name: str,
        agent_id: str,
        agent_name: str,
    ) -> None: ...

    @abstractmethod
    async def retrieve_default_categories(
        self, user_id: str, agent_id: str
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


class MemuSdkMemoryStore(MemoryStore):
    def __init__(self, env: AsyncTenEnv, base_url: str, api_key: str):
        super().__init__(env)
        from memu.sdk.python import MemuClient

        self.client = MemuClient(base_url=base_url, api_key=api_key)

    async def memorize(
        self,
        conversation: list[dict],
        user_id: str,
        user_name: str,
        agent_id: str,
        agent_name: str,
    ) -> None:
        resp = self.client.memorize_conversation(
            conversation=conversation,
            user_id=user_id,
            user_name=user_name,
            agent_id=agent_id,
            agent_name=agent_name,
        )
        # wait until finished
        while True:
            status = self.client.get_task_status(resp.task_id)
            if status.status in ["SUCCESS", "FAILURE", "REVOKED"]:
                break
            await asyncio.sleep(2)

    async def retrieve_default_categories(
        self, user_id: str, agent_id: str
    ) -> Any:
        return self.client.retrieve_default_categories(user_id=user_id)

    def parse_default_categories(self, data: Any) -> dict:
        # Assume SDK already returns a summary-like object with attributes or dict-compatible
        if isinstance(data, dict):
            return data
        # Convert SDK object to dict structure
        categories = getattr(data, "categories", [])
        summary = {
            "basic_stats": {
                "total_categories": len(categories),
                "total_memories": sum(
                    getattr(cat, "memory_count", 0) or 0 for cat in categories
                ),
                "user_id": (
                    getattr(categories[0], "user_id", None)
                    if categories
                    else None
                ),
                "agent_id": (
                    getattr(categories[0], "agent_id", None)
                    if categories
                    else None
                ),
            },
            "categories": [],
        }
        for category in categories:
            cat_summary = {
                "name": getattr(category, "name", None),
                "type": getattr(category, "type", None),
                "memory_count": getattr(category, "memory_count", 0) or 0,
                "is_active": getattr(category, "is_active", None),
                "recent_memories": [],
                "summary": getattr(category, "summary", None),
            }
            memories = getattr(category, "memories", None)
            if memories is None:
                memories = getattr(category, "recent_memories", [])
            for memory in memories or []:
                happened = getattr(memory, "happened_at", None)
                date_str = None
                try:
                    date_str = (
                        happened.strftime("%Y-%m-%d %H:%M")
                        if hasattr(happened, "strftime")
                        else str(happened)
                    )
                except Exception:
                    date_str = str(happened)
                content = getattr(memory, "content", None)
                cat_summary["recent_memories"].append(
                    {"date": date_str, "content": content}
                )
            summary["categories"].append(cat_summary)
        return summary


class MemuHttpMemoryStore(MemoryStore):
    def __init__(
        self, env: AsyncTenEnv, base_url: str, api_key: str | None = ""
    ):
        super().__init__(env)
        import aiohttp

        self.base_url = base_url.rstrip("/")
        self.api_key = api_key or ""

    async def _headers(self) -> dict[str, str]:
        headers = {"Content-Type": "application/json"}
        if self.api_key:
            headers["Authorization"] = f"Bearer {self.api_key}"
        return headers

    async def memorize(
        self,
        conversation: list[dict],
        user_id: str,
        user_name: str,
        agent_id: str,
        agent_name: str,
    ) -> None:
        import aiohttp

        payload = {
            "conversation": conversation,
            "user_id": user_id,
            "user_name": user_name,
            "agent_id": agent_id,
            "agent_name": agent_name,
        }
        async with aiohttp.ClientSession() as session:
            async with session.post(
                f"{self.base_url}/api/v1/memory/memorize",
                headers=await self._headers(),
                data=json.dumps(payload),
            ) as r:
                r.raise_for_status()
                data = await r.json()
                task_id = data.get("task_id")
        # poll
        async with aiohttp.ClientSession() as session:
            while True:
                async with session.get(
                    f"{self.base_url}/api/v1/memory/memorize/status/{task_id}",
                    headers=await self._headers(),
                ) as r:
                    r.raise_for_status()
                    data = await r.json()
                    status = data.get("status") or data.get("state")
                    if status in [
                        "SUCCESS",
                        "FAILURE",
                        "REVOKED",
                        "DONE",
                        "ERROR",
                    ]:
                        break
                await asyncio.sleep(2)

    async def retrieve_default_categories(
        self, user_id: str, agent_id: str
    ) -> Any:
        import aiohttp

        payload = {"user_id": user_id, "agent_id": agent_id}
        async with aiohttp.ClientSession() as session
            async with session.post(
                f"{self.base_url}/api/v1/memory/retrieve/default-categories",
                headers=await self._headers(),
                data=json.dumps(payload),
            ) as r:
                r.raise_for_status()
                return await r.json()

    def parse_default_categories(self, data: Any) -> dict:
        # HTTP returns raw categories with memories. Normalize to summary schema.
        if not isinstance(data, dict):
            return {
                "basic_stats": {"total_categories": 0, "total_memories": 0},
                "categories": [],
            }

        self.env.log_info(
            f"MemuHttpMemoryStore: parse_default_categories: {data}"
        )

        categories = data.get("categories", [])
        total_memories = 0
        out_categories = []
        for cat in categories:
            memories = cat.get("memories", []) or []
            total_memories += len(memories)
            out_cat = {
                "name": cat.get("name"),
                "type": cat.get("type"),
                "memory_count": cat.get("memory_count") or len(memories),
                "is_active": cat.get("is_active"),
                "recent_memories": [],
                "summary": None,
            }
            for m in memories:
                out_cat["recent_memories"].append(
                    {
                        "date": str(m.get("happened_at")),
                        "content": m.get("content"),
                    }
                )
            out_categories.append(out_cat)
        return {
            "basic_stats": {
                "total_categories": len(categories),
                "total_memories": total_memories,
                "user_id": None,
                "agent_id": None,
            },
            "categories": out_categories,
        }
