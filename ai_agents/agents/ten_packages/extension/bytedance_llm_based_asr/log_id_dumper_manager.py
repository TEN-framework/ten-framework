#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import os
from typing import TYPE_CHECKING

from ten_ai_base.dumper import Dumper

if TYPE_CHECKING:
    from ten_runtime import AsyncTenEnv
    from .config import BytedanceASRLLMConfig


class LogIdDumperManager:
    """Manager for log_id-based audio dumping.

    Dumps audio to out_init.pcm before first log_id is received.
    When log_id arrives, stops init dumper and creates out_<log_id>.pcm dumper.
    When log_id changes, stops old dumper and creates new one.
    """

    def __init__(self, config: "BytedanceASRLLMConfig", ten_env: "AsyncTenEnv") -> None:
        self.config = config
        self.ten_env = ten_env
        self.current_log_id: str | None = None
        self.init_dumper: Dumper | None = None  # For audio before first log_id
        self.log_id_dumper: Dumper | None = None  # For audio after log_id

    async def create_init_dumper(self) -> None:
        """Create init dumper for new connection (before first log_id)."""
        # Stop any existing dumpers
        await self.stop_all()

        # Reset state
        self.current_log_id = None

        # Create init dumper for audio before first log_id
        if self.config and self.config.dump:
            init_dump_file_path = os.path.join(self.config.dump_path, "out_init.pcm")
            self.init_dumper = Dumper(init_dump_file_path)
            await self.init_dumper.start()
            self.ten_env.log_info(f"Created init dumper: {init_dump_file_path}")

    async def update_log_id(self, log_id: str) -> None:
        """Update log_id and create new dumper.

        When log_id is received for the first time or changes:
        - Stop init_dumper if it exists
        - Stop old log_id_dumper if log_id changed
        - Create new log_id_dumper with the new log_id
        """
        if not log_id or not isinstance(log_id, str):
            return

        # Check if log_id has changed
        if self.current_log_id == log_id:
            return

        self.ten_env.log_info(f"Updating log_id from {self.current_log_id} to {log_id}")

        # Stop init dumper if exists (first log_id received)
        if self.init_dumper:
            try:
                await self.init_dumper.stop()
                self.ten_env.log_info("Stopped init dumper")
            except Exception as e:
                self.ten_env.log_error(f"Error stopping init_dumper: {e}")
            finally:
                self.init_dumper = None

        # Stop old log_id_dumper if exists (log_id changed)
        if self.log_id_dumper:
            try:
                await self.log_id_dumper.stop()
                self.ten_env.log_info(
                    f"Stopped dumper for log_id: {self.current_log_id}"
                )
            except Exception as e:
                self.ten_env.log_error(f"Error stopping log_id_dumper: {e}")
            finally:
                self.log_id_dumper = None

        # Update current log_id
        self.current_log_id = log_id

        # Create new log_id_dumper
        if self.config and self.config.dump:
            log_id_dump_file_path = os.path.join(
                self.config.dump_path, f"out_{log_id}.pcm"
            )
            # Ensure directory exists
            os.makedirs(self.config.dump_path, exist_ok=True)

            self.log_id_dumper = Dumper(log_id_dump_file_path)
            await self.log_id_dumper.start()
            self.ten_env.log_info(f"Created log_id dumper: {log_id_dump_file_path}")

    async def push_bytes(self, data: bytes) -> None:
        """Push bytes to appropriate dumper.

        Before first log_id: write to init_dumper
        After log_id received: write to log_id_dumper
        """
        # Write to log_id_dumper if available (log_id received)
        if self.log_id_dumper:
            await self.log_id_dumper.push_bytes(data)
        # Otherwise write to init_dumper (before first log_id)
        elif self.init_dumper:
            await self.init_dumper.push_bytes(data)

    async def stop_all(self) -> None:
        """Stop all dumpers."""
        # Stop init dumper if exists
        if self.init_dumper:
            try:
                await self.init_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(f"Error stopping init_dumper: {e}")
            finally:
                self.init_dumper = None

        # Stop log_id dumper if exists
        if self.log_id_dumper:
            try:
                await self.log_id_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(f"Error stopping log_id_dumper: {e}")
            finally:
                self.log_id_dumper = None

        # Reset state
        self.current_log_id = None
