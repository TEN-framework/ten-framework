import asyncio
from typing import Callable, Awaitable, Optional
from ten_ai_base.message import ModuleError, ModuleErrorCode
from .const import MODULE_NAME_ASR


class ReconnectManager:
    """
    Manages reconnection attempts with unlimited retries and exponential backoff.

    Backoff sequence: 0.5s, 1s, 2s, 4s (capped).
    """

    def __init__(
        self,
        base_delay: float = 0.5,
        max_delay: float = 4.0,
        logger=None,
    ):
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.logger = logger

        self.attempts = 0
        self._connection_successful = False

    def _reset_counter(self):
        self.attempts = 0
        if self.logger:
            self.logger.log_debug("Reconnect counter reset")

    def mark_connection_successful(self):
        self._connection_successful = True
        self._reset_counter()

    def get_attempts_info(self) -> dict:
        return {
            "current_attempts": self.attempts,
            "unlimited_retries": True,
        }

    async def handle_reconnect(
        self,
        connection_func: Callable[[], Awaitable[None]],
        error_handler: Optional[
            Callable[[ModuleError], Awaitable[None]]
        ] = None,
    ) -> bool:
        self._connection_successful = False
        self.attempts += 1

        delay = min(
            self.base_delay * (2 ** (self.attempts - 1)), self.max_delay
        )

        if self.logger:
            self.logger.log_warn(
                f"Attempting reconnection #{self.attempts} "
                f"after {delay:.2f} seconds delay..."
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
                    f"Reconnection attempt #{self.attempts} failed: {e}. Will retry..."
                )

            if error_handler:
                await error_handler(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=f"Reconnection attempt #{self.attempts} failed: {str(e)}",
                    )
                )

            return False
