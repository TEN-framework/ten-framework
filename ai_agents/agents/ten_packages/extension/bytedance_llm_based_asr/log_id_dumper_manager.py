#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import os
import uuid
from typing import TYPE_CHECKING

from ten_ai_base.dumper import Dumper

if TYPE_CHECKING:
    from ten_runtime import AsyncTenEnv
    from .config import BytedanceASRLLMConfig


class LogIdDumperManager:
    """Manager for log_id-based audio dumping.

    Dumps audio to out_{uuid}.pcm initially.
    When log_id arrives, renames the file to out_{log_id}.pcm and continues writing.
    When connection ends, closes the file.
    """

    def __init__(
        self, config: "BytedanceASRLLMConfig", ten_env: "AsyncTenEnv"
    ) -> None:
        self.config = config
        self.ten_env = ten_env
        self.current_log_id: str | None = None
        self.temp_uuid: str | None = None
        self.dumper: Dumper | None = None
        self.current_file_path: str | None = None

    async def create_dumper(self) -> None:
        """Create dumper for new connection with temporary UUID filename."""
        # Stop any existing dumper
        await self.stop()

        # Reset state
        self.current_log_id = None

        # Generate UUID for temporary filename
        self.temp_uuid = str(uuid.uuid4())

        # Create dumper with UUID filename
        if self.config and self.config.dump:
            # Ensure directory exists
            os.makedirs(self.config.dump_path, exist_ok=True)

            self.current_file_path = os.path.join(
                self.config.dump_path, f"out_{self.temp_uuid}.pcm"
            )
            self.dumper = Dumper(self.current_file_path)
            await self.dumper.start()
            self.ten_env.log_info(
                f"Created dumper with temp file: {self.current_file_path}"
            )

    async def update_log_id(self, log_id: str) -> None:
        """Update log_id and rename the file.

        When log_id is received for the first time:
        - Stop current dumper to flush and close file
        - Rename file from out_{uuid}.pcm to out_{log_id}.pcm
        - Reopen dumper with renamed file path

        If log_id changes to a different value (rare), log warning but don't rename again.
        """
        if not log_id or not isinstance(log_id, str):
            return

        # If log_id already set and same, do nothing
        if self.current_log_id == log_id:
            return

        # If log_id changes to a different value, log warning
        if self.current_log_id is not None and self.current_log_id != log_id:
            self.ten_env.log_warn(
                f"log_id changed from {self.current_log_id} to {log_id}, "
                f"keeping current file: {self.current_file_path}"
            )
            return

        self.ten_env.log_info(
            f"Received first log_id: {log_id}, renaming file..."
        )

        # Stop current dumper to flush and close file
        if self.dumper:
            try:
                await self.dumper.stop()
                self.ten_env.log_info("Stopped dumper for renaming")
            except Exception as e:
                self.ten_env.log_error(f"Error stopping dumper: {e}")
            finally:
                self.dumper = None

        # Rename file
        if self.current_file_path and os.path.exists(self.current_file_path):
            new_file_path = os.path.join(
                self.config.dump_path, f"out_{log_id}.pcm"
            )
            try:
                os.rename(self.current_file_path, new_file_path)
                self.ten_env.log_info(
                    f"Renamed file from {self.current_file_path} to {new_file_path}"
                )
                self.current_file_path = new_file_path
            except Exception as e:
                self.ten_env.log_error(f"Error renaming file: {e}")
                # Keep old path if rename failed
                new_file_path = self.current_file_path
        else:
            # If file doesn't exist, create new path with log_id
            new_file_path = os.path.join(
                self.config.dump_path, f"out_{log_id}.pcm"
            )
            self.current_file_path = new_file_path

        # Update log_id
        self.current_log_id = log_id

        # Reopen dumper with renamed file (append mode)
        if self.config and self.config.dump and self.current_file_path:
            self.dumper = Dumper(self.current_file_path)
            await self.dumper.start()
            self.ten_env.log_info(
                f"Reopened dumper with file: {self.current_file_path}"
            )

    async def push_bytes(self, data: bytes) -> None:
        """Push bytes to dumper."""
        if self.dumper:
            try:
                await self.dumper.push_bytes(data)
            except Exception as e:
                # Dumper might be temporarily closed during rename operation
                # Log warning but don't fail - this is just for debugging/dumping
                self.ten_env.log_warn(
                    f"Error pushing bytes to dumper (may be temporarily closed): {e}"
                )

    async def stop(self) -> None:
        """Stop dumper and close file."""
        if self.dumper:
            try:
                await self.dumper.stop()
                self.ten_env.log_info(
                    f"Stopped dumper: {self.current_file_path}"
                )
            except Exception as e:
                self.ten_env.log_error(f"Error stopping dumper: {e}")
            finally:
                self.dumper = None

        # Reset state
        self.current_log_id = None
        self.temp_uuid = None
        self.current_file_path = None
