import asyncio
from collections.abc import Awaitable, Callable

from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleType


class ReconnectManager:
    """
    Manages reconnection attempts with fixed retry limit and exponential backoff.

    - Fixed retry limit (default: 5 attempts)
    - Exponential backoff: 300ms, 600ms, 1.2s, 2.4s, 4.8s
    - Counter resets after successful connection
    """

    def __init__(
        self,
        max_attempts: int = 5,
        base_delay: float = 0.3,
        logger=None,
    ):
        self.max_attempts = max_attempts
        self.base_delay = base_delay
        self.logger = logger

        self.attempts = 0
        self._connection_successful = False

    def reset_counter(self) -> None:
        self.attempts = 0
        if self.logger:
            self.logger.log_debug("Reconnect counter reset")

    def mark_connection_successful(self) -> None:
        self._connection_successful = True
        self.reset_counter()

    def can_retry(self) -> bool:
        return self.attempts < self.max_attempts

    def get_attempts_info(self) -> dict:
        return {
            "current_attempts": self.attempts,
            "max_attempts": self.max_attempts,
            "can_retry": self.can_retry(),
        }

    async def handle_reconnect(
        self,
        connection_func: Callable[[], Awaitable[None]],
        error_handler: Callable[[ModuleError], Awaitable[None]] | None = None,
    ) -> bool:
        if not self.can_retry():
            if self.logger:
                self.logger.log_error(
                    f"Maximum reconnection attempts ({self.max_attempts}) reached. No more attempts allowed."
                )
            if error_handler:
                await error_handler(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=f"Failed to reconnect after {self.max_attempts} attempts",
                    )
                )
            return False

        self._connection_successful = False
        self.attempts += 1

        delay = self.base_delay * (2 ** (self.attempts - 1))
        if self.logger:
            self.logger.log_warn(
                f"Attempting reconnection #{self.attempts}/{self.max_attempts} after {delay} seconds delay..."
            )

        try:
            await asyncio.sleep(delay)
            await connection_func()
            if self.logger:
                self.logger.log_debug(
                    f"Connection function completed for attempt #{self.attempts}"
                )
            return True
        except Exception as e:
            if self.logger:
                self.logger.log_error(
                    f"Reconnection attempt #{self.attempts} failed: {e}"
                )
            if self.attempts >= self.max_attempts and error_handler:
                await error_handler(
                    ModuleError(
                        module=ModuleType.ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=f"All reconnection attempts failed. Last error: {str(e)}",
                    )
                )
            return False
