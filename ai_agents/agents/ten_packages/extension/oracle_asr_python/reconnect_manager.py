import asyncio
from typing import Callable, Awaitable, Optional
from ten_ai_base.message import ModuleError, ModuleErrorCode
from .const import MODULE_NAME_ASR


class ReconnectManager:
    """
    Manages reconnection attempts with exponential backoff and a
    configurable maximum number of retries (default 5).

    Backoff sequence: 0.5s, 1s, 2s, 4s (capped at max_delay).
    After max_attempts consecutive failures a FATAL_ERROR is reported.
    """

    def __init__(
        self,
        base_delay: float = 0.5,
        max_delay: float = 4.0,
        max_attempts: int = 5,
        logger=None,
    ):
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.max_attempts = max_attempts
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
            "max_attempts": self.max_attempts,
        }

    def can_retry(self) -> bool:
        return self.attempts < self.max_attempts

    async def handle_reconnect(
        self,
        connection_func: Callable[[], Awaitable[None]],
        error_handler: Optional[
            Callable[[ModuleError], Awaitable[None]]
        ] = None,
    ) -> bool:
        if not self.can_retry():
            if self.logger:
                self.logger.log_error(
                    "Reconnection attempts exhausted"
                )
            if error_handler:
                await error_handler(
                    ModuleError(
                        module=MODULE_NAME_ASR,
                        code=ModuleErrorCode.FATAL_ERROR.value,
                        message=(
                            "Maximum reconnection attempts reached. "
                            "Please check network connectivity and OCI credentials."
                        ),
                    )
                )
            return False

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

            if not self._connection_successful:
                if self.logger:
                    self.logger.log_warn(
                        f"Reconnection attempt #{self.attempts} did not establish a connection"
                    )
                return False

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
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message=f"Reconnection attempt #{self.attempts} failed: {str(e)}",
                    )
                )

            return False
