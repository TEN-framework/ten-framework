from typing import Callable
from .events import AgentEvent


def agent_event_handler(event_type: str) -> Callable:
    """
    Decorator to mark a method as an agent event handler.

    Usage:
        @agent_event_handler(ASRResultEvent)
        async def handle_asr_result(self, event: ASRResultEvent):
            ...

    Args:
        event_type: The type of event to handle (class or string)

    Returns:
        Decorated function
    """

    def decorator(func: Callable) -> Callable:
        # Store the event type on the function for later registration
        func._agent_event_type = (
            event_type if isinstance(event_type, str) else event_type.type
        )
        return func

    return decorator