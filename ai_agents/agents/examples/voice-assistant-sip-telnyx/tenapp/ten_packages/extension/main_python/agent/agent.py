import asyncio
import json
from typing import Callable, Dict, Any, Optional
from ten_runtime import AsyncTenEnv

from .events import (
    AgentEvent,
    ASRResultEvent,
    LLMResponseEvent,
    ToolRegisterEvent,
)


class Agent:
    """
    Agent class for handling LLM interactions.
    This is a simplified agent that can be extended with more capabilities.
    """

    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env
        self.event_handlers: Dict[str, list[Callable]] = {}
        self._llm_queue: asyncio.Queue = asyncio.Queue()
        self._running = False
        self._llm_task: Optional[asyncio.Task] = None

    def on(self, event_type: str, handler: Callable):
        """Register an event handler"""
        if event_type not in self.event_handlers:
            self.event_handlers[event_type] = []
        self.event_handlers[event_type].append(handler)

    async def emit(self, event: AgentEvent):
        """Emit an event to all registered handlers"""
        event_type = event.type
        if event_type in self.event_handlers:
            for handler in self.event_handlers[event_type]:
                try:
                    await handler(event)
                except Exception as e:
                    self.ten_env.log_error(f"Error in event handler: {e}")

    async def queue_llm_input(self, text: str):
        """Queue text for LLM processing"""
        await self._llm_queue.put(text)

    async def flush_llm(self):
        """Flush pending LLM inputs"""
        while not self._llm_queue.empty():
            try:
                self._llm_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

    async def start(self):
        """Start the agent's background tasks"""
        self._running = True
        self._llm_task = asyncio.create_task(self._process_llm_queue())

    async def stop(self):
        """Stop the agent's background tasks"""
        self._running = False
        if self._llm_task:
            self._llm_task.cancel()
            try:
                await self._llm_task
            except asyncio.CancelledError:
                pass

    async def _process_llm_queue(self):
        """Process text from the LLM queue"""
        while self._running:
            try:
                text = await asyncio.wait_for(
                    self._llm_queue.get(), timeout=1.0
                )
                await self._process_text(text)
            except asyncio.TimeoutError:
                continue
            except asyncio.CancelledError:
                break
            except Exception as e:
                self.ten_env.log_error(f"Error processing LLM queue: {e}")

    async def _process_text(self, text: str):
        """Process text - this is a placeholder that should be extended"""
        # This would typically call an LLM API
        # For now, just emit a placeholder response
        response_event = LLMResponseEvent(
            type="llm_response",
            text=f"I heard: {text}",
            delta=f"I heard: {text}",
            is_final=True,
        )
        await self.emit(response_event)

    async def on_cmd(self, cmd):
        """Handle commands"""
        # Placeholder for command handling
        pass

    async def on_data(self, data):
        """Handle data"""
        # Placeholder for data handling
        pass

    async def register_llm_tool(self, tool: Dict[str, Any], source: str):
        """Register a tool for LLM use"""
        # Placeholder for tool registration
        pass