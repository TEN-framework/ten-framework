import asyncio
from typing import Callable, Awaitable, Optional
from ten_ai_base.message import (
    ModuleError,
    ModuleErrorCode,
    ModuleErrorVendorInfo,
)
from .const import MODULE_NAME_ASR


class ReconnectManager:
    """
    Manages bounded reconnection attempts with exponential backoff.

    Features:
    - Bounded retry attempts (caller chooses `max_attempts`; default 4).
      The xAI ASR extension overrides this to 10 because the audio
      buffer holds 10 MB and can tolerate longer outages.
    - Exponential backoff: `base_delay * 2^(attempts-1)`, capped at `max_delay`
      (default 0.5s, 1s, 2s, 4s, then 4s thereafter).
    - Automatic counter reset after a successful connection (via
      `mark_connection_successful`).
    - Detailed logging for monitoring and debugging.
    """

    def __init__(
        self,
        base_delay: float = 0.5,  # 500 milliseconds
        max_delay: float = 4.0,  # 4 seconds maximum delay
        max_attempts: int = 4,
        logger=None,
    ):
        self.base_delay = base_delay
        self.max_delay = max_delay
        self.max_attempts = max_attempts
        self.logger = logger

        # State tracking
        self.attempts = 0
        self._connection_successful = False

    def _reset_counter(self):
        """Reset reconnection counter"""
        self.attempts = 0
        if self.logger:
            self.logger.log_debug("Reconnect counter reset")

    def mark_connection_successful(self):
        """Mark connection as successful and reset counter"""
        self._connection_successful = True
        self._reset_counter()

    def get_attempts_info(self) -> dict:
        """Get current reconnection attempts information"""
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
        vendor_name: str | None = None,
        vendor_code: str = "connect_failed",
    ) -> bool:
        """
        Handle a single reconnection attempt with backoff delay.

        Args:
            connection_func: Async function to establish connection
            error_handler: Optional async function to handle errors

        Returns:
            True if connection function executed successfully, False if attempt failed
            Note: Actual connection success is determined by callback calling mark_connection_successful()
        """
        self._connection_successful = False
        self.attempts += 1

        # Calculate exponential backoff delay with max limit: min(2^(attempts-1) * base_delay, max_delay)
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

            # Connection function completed successfully
            # Actual connection success will be determined by callback
            if self.logger:
                self.logger.log_debug(
                    f"Connection function completed for attempt #{self.attempts}"
                )
            return True

        except Exception as e:
            is_fatal = self.attempts >= self.max_attempts
            if self.logger:
                self.logger.log_error(
                    f"Reconnection attempt #{self.attempts} failed: {e}. "
                    f"{'Giving up.' if is_fatal else 'Will retry...'}"
                )

            if error_handler:
                error = ModuleError(
                    module=MODULE_NAME_ASR,
                    code=(
                        ModuleErrorCode.FATAL_ERROR.value
                        if is_fatal
                        else ModuleErrorCode.NON_FATAL_ERROR.value
                    ),
                    message=f"Reconnection attempt #{self.attempts} failed: {str(e)}",
                )
                vendor_info = (
                    ModuleErrorVendorInfo(
                        vendor=vendor_name,
                        code=vendor_code,
                        message=str(e),
                    )
                    if vendor_name
                    else None
                )
                if vendor_info is not None:
                    await error_handler(error, vendor_info)
                else:
                    await error_handler(error)

            return False
