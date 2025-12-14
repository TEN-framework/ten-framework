#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import os
import uuid
import time
import aiofiles
from typing import TYPE_CHECKING

from ten_ai_base.dumper import Dumper

if TYPE_CHECKING:
    from ten_runtime import AsyncTenEnv
    from .config import BytedanceASRLLMConfig


class RenamableDumper(Dumper):
    """Extended Dumper with rename and append mode support."""

    def __init__(self, dump_file_path: str, append: bool = False):
        super().__init__(dump_file_path)
        self.append: bool = append

    async def start(self):
        if self._file:
            return

        dir_path = os.path.dirname(self.dump_file_path)
        if dir_path:  # Only create directory if path is not empty
            os.makedirs(dir_path, exist_ok=True)

        mode = "ab" if self.append else "wb"
        self._file = await aiofiles.open(self.dump_file_path, mode=mode)

    async def rename_file(self, new_path: str) -> bool:
        """
        Rename the dump file to a new path.
        The dumper must be stopped before calling this method.

        Args:
            new_path: The new file path (may be modified if target exists)

        Returns:
            True if rename succeeded, False otherwise
        """
        if self._file:
            raise RuntimeError(
                "Dumper must be stopped before renaming. Current path: {}".format(
                    self.dump_file_path
                )
            )

        if not os.path.exists(self.dump_file_path):
            return False

        try:
            # Ensure target directory exists
            dir_path = os.path.dirname(new_path)
            if dir_path:  # Only create directory if path is not empty
                os.makedirs(dir_path, exist_ok=True)

            # Handle case where target file already exists
            final_path = new_path
            if os.path.exists(new_path):
                base, ext = os.path.splitext(new_path)
                counter = 1
                while os.path.exists(f"{base}_{counter}{ext}"):
                    counter += 1
                final_path = f"{base}_{counter}{ext}"

            os.rename(self.dump_file_path, final_path)
            self.dump_file_path = final_path
            return True
        except Exception as e:
            # Log the exception for debugging but don't expose it
            return False


class LogIdDumperManager:
    """Manager for log_id-based dumper with dual pointer approach."""

    def __init__(
        self, config: "BytedanceASRLLMConfig", ten_env: "AsyncTenEnv"
    ) -> None:
        self.config = config
        self.ten_env = ten_env
        self.final_log_id: str | None = None
        # Dual pointer for log_id_dumper to avoid concurrent write issues
        self.current_log_id_dumper: RenamableDumper | None = None
        self.previous_log_id_dumper: RenamableDumper | None = None
        # Flag to prevent concurrent rename operations
        self._renaming: bool = False

    def _needs_rename(self) -> bool:
        """Check if rename is needed based on current file path."""
        if not self.current_log_id_dumper or not self.final_log_id:
            return False
        # Check if current file is still a temp file (needs rename)
        current_path = self.current_log_id_dumper.dump_file_path
        return "out_temp_" in os.path.basename(current_path)

    async def create_temp_dumper(self) -> None:
        """Create a temporary dumper for new connection."""
        # Stop old log_id_dumpers if exist (for reconnection)
        # Stop previous dumper if exists
        if self.previous_log_id_dumper:
            try:
                await self.previous_log_id_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error stopping previous_log_id_dumper: {e}"
                )
            finally:
                self.previous_log_id_dumper = None

        # Stop current dumper if exists
        if self.current_log_id_dumper:
            try:
                await self.current_log_id_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error stopping current_log_id_dumper: {e}"
                )
            finally:
                self.current_log_id_dumper = None

        # Reset renaming flag
        self._renaming = False

        # Create temporary log_id_dumper for new connection
        if self.config and self.config.dump:
            temp_uuid = str(uuid.uuid4())
            temp_dump_file_path = os.path.join(
                self.config.dump_path, f"out_temp_{temp_uuid}.pcm"
            )
            # Initialize final_log_id with current timestamp (milliseconds)
            self.final_log_id = str(int(time.time() * 1000))

            self.current_log_id_dumper = RenamableDumper(temp_dump_file_path)
            await self.current_log_id_dumper.start()
            self.ten_env.log_info(
                f"Created temporary log_id_dumper: {temp_dump_file_path}, initial log_id: {self.final_log_id}"
            )

    def update_log_id(self, log_id: str) -> None:
        """Update log_id when received from server."""
        if self._needs_rename() and log_id and isinstance(log_id, str):
            self.final_log_id = log_id
            self.ten_env.log_info(
                f"Received log_id: {log_id}, will rename file when needed"
            )

    async def perform_rename_if_needed(self) -> None:
        """Perform file rename operation when log_id is received.

        Uses dual pointer approach:
        - Stop previous_log_id_dumper if exists
        - Rename current_log_id_dumper's file
        - Create new dumper and swap pointers
        """
        # Fast path: check conditions
        if not self.current_log_id_dumper or not self.final_log_id:
            return

        # Check if rename is needed
        if not self._needs_rename():
            return

        # Prevent concurrent rename operations
        if self._renaming:
            return
        self._renaming = True

        try:
            # Step 1: Stop previous dumper if exists (cleanup old one)
            if self.previous_log_id_dumper:
                try:
                    await self.previous_log_id_dumper.stop()
                except Exception as e:
                    self.ten_env.log_error(
                        f"Error stopping previous_log_id_dumper: {e}"
                    )
                finally:
                    self.previous_log_id_dumper = None

            # Step 2: Get reference to current dumper and file path
            old_dumper = self.current_log_id_dumper
            old_file_path = old_dumper.dump_file_path

            # Step 3: Build new file path
            if not self.config:
                return
            new_file_path = os.path.join(
                self.config.dump_path, f"out_{self.final_log_id}.pcm"
            )
            # Ensure target directory exists
            os.makedirs(self.config.dump_path, exist_ok=True)

            # Step 4: Stop current dumper (this closes the file handle)
            await old_dumper.stop()

            # Step 5: Rename file
            rename_success = await old_dumper.rename_file(new_file_path)
            if not rename_success:
                self.ten_env.log_error(
                    f"Failed to rename dump file from {old_file_path} to {new_file_path}"
                )
                # Continue with old path (file still exists at old location)
                new_file_path = old_file_path
            else:
                self.ten_env.log_info(
                    f"Renamed log_id_dumper file to: {new_file_path}"
                )

            # Step 6: Create new dumper pointing to renamed file (append mode)
            new_dumper = RenamableDumper(new_file_path, append=True)
            await new_dumper.start()

            # Step 7: Swap pointers atomically
            # Move current to previous, new becomes current
            self.previous_log_id_dumper = old_dumper
            self.current_log_id_dumper = new_dumper

        except Exception as e:
            self.ten_env.log_error(f"Error during rename operation: {e}")
            # Try to recreate current_log_id_dumper with old path (use append mode since file exists)
            if old_dumper and old_dumper.dump_file_path:
                try:
                    self.current_log_id_dumper = RenamableDumper(
                        old_dumper.dump_file_path, append=True
                    )
                    await self.current_log_id_dumper.start()
                except Exception as recreate_error:
                    self.ten_env.log_error(
                        f"Failed to recreate current_log_id_dumper: {recreate_error}"
                    )
                    self.current_log_id_dumper = None
            else:
                self.current_log_id_dumper = None
        finally:
            self._renaming = False

    async def push_bytes(self, data: bytes) -> None:
        """Push bytes to current log_id_dumper if enabled."""
        # Perform rename if needed (before writing new audio)
        await self.perform_rename_if_needed()

        # Dump audio to current_log_id_dumper if enabled
        # Using current_log_id_dumper ensures we always write to the active dumper
        if self.current_log_id_dumper:
            await self.current_log_id_dumper.push_bytes(data)

    async def stop_all(self) -> None:
        """Stop all log_id_dumpers."""
        # Perform rename if needed
        await self.perform_rename_if_needed()

        # Stop log_id_dumpers if exist
        if self.previous_log_id_dumper:
            try:
                await self.previous_log_id_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error stopping previous_log_id_dumper: {e}"
                )
            finally:
                self.previous_log_id_dumper = None

        if self.current_log_id_dumper:
            try:
                await self.current_log_id_dumper.stop()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error stopping current_log_id_dumper: {e}"
                )
            finally:
                self.current_log_id_dumper = None
