import asyncio
from typing import Awaitable, Callable, Optional


class ReconnectManager:
    """Own one bounded-backoff reconnect policy.

    ``max_attempts=0`` means retry indefinitely.  A reconnect loop owned by the
    extension calls this manager once per attempt; this class never starts a
    second loop or emits framework errors on its own.
    """

    def __init__(
        self,
        max_attempts: int = 0,
        base_delay: float = 0.5,
        max_delay: float = 4.0,
        logger=None,
    ):
        if max_attempts < 0:
            raise ValueError("max_attempts must be >= 0")
        if base_delay <= 0 or max_delay < base_delay:
            raise ValueError("invalid reconnect delay configuration")
        self.max_attempts = max_attempts
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.logger = logger
        self.attempts = 0
        self._next_delay = base_delay

    def reset_counter(self) -> None:
        self.attempts = 0
        self._next_delay = self.base_delay

    def mark_connection_successful(self) -> None:
        self.reset_counter()

    def can_retry(self) -> bool:
        return self.max_attempts == 0 or self.attempts < self.max_attempts

    def get_attempts_info(self) -> dict[str, int | bool]:
        return {
            "current_attempts": self.attempts,
            "max_attempts": self.max_attempts,
            "can_retry": self.can_retry(),
        }

    async def handle_reconnect(
        self,
        connection_func: Callable[[], Awaitable[None]],
        error_handler: Optional[Callable[[Exception], Awaitable[None]]] = None,
    ) -> bool:
        """Wait with backoff and run exactly one connection attempt."""
        if not self.can_retry():
            return False

        self.attempts += 1
        delay = self._next_delay
        self._next_delay = min(delay * 2, self.max_delay)
        if self.logger:
            self.logger.log_warn(
                f"Attempting ElevenLabs reconnection #{self.attempts} "
                f"after {delay:.2f} seconds"
            )

        try:
            await asyncio.sleep(delay)
            await connection_func()
            return True
        except Exception as exc:
            if self.logger:
                self.logger.log_warn(
                    f"ElevenLabs reconnect attempt failed: {exc}"
                )
            if error_handler:
                await error_handler(exc)
            return False
