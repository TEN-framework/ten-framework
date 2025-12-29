#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import aiofiles
import asyncio
import os
import secrets
import time


class Dumper:
    def __init__(self, dump_dir_path: str, dump_file_name: str):
        self.dump_dir_path = dump_dir_path
        self.dump_file_name = dump_file_name
        self._current_file_name = dump_file_name
        self._file: aiofiles.threadpool.binary.AsyncBufferedIOBase | None = None
        self._lock: asyncio.Lock = asyncio.Lock()

    async def start(self):
        async with self._lock:
            if self._file:
                return

            self._ensure_dir()

            self._file = await aiofiles.open(
                self._current_file_path(), mode="wb"
            )

    def _ensure_dir(self):
        os.makedirs(self.dump_dir_path, exist_ok=True)

    def _current_file_path(self) -> str:
        return os.path.join(self.dump_dir_path, self._current_file_name)

    async def stop(self):
        async with self._lock:
            if self._file:
                await self._file.close()
                self._file = None

    async def rotate(self):
        """Close current file and open new one with timestamp suffix."""
        async with self._lock:
            old_file = self._file

            try:
                # Generate new filename
                base, ext = os.path.splitext(self.dump_file_name)
                self._current_file_name = f"{base}_{int(time.time() * 1000)}_{secrets.token_hex(3)}{ext}"

                # Open new file first
                self._ensure_dir()
                new_file = await aiofiles.open(
                    self._current_file_path(), mode="wb"
                )

                # Only close old file after new one opens successfully
                if old_file:
                    await old_file.close()

                self._file = new_file
            except Exception as e:
                # Keep old file handle if rotation fails
                self._file = old_file
                raise RuntimeError(f"Failed to rotate dump file: {e}") from e

    async def push_bytes(self, data: bytes):
        async with self._lock:
            if not self._file:
                raise RuntimeError(
                    f"Dumper for {self._current_file_path()} is not opened. Please start the Dumper first."
                )
            _ = await self._file.write(data)
