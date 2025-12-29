#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import aiofiles
import asyncio
import os
import secrets
from datetime import datetime


class Dumper:
    def __init__(self, dump_file_path: str):
        self.dump_file_base_path: str = dump_file_path
        self.dump_file_path: str = dump_file_path
        self._file: aiofiles.threadpool.binary.AsyncBufferedIOBase | None = None
        self._lock: asyncio.Lock = asyncio.Lock()

    async def start(self):
        if self._file:
            return

        os.makedirs(os.path.dirname(self.dump_file_path), exist_ok=True)

        self._file = await aiofiles.open(self.dump_file_path, mode="wb")

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
                timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
                base, ext = os.path.splitext(self.dump_file_base_path)
                self.dump_file_path = (
                    f"{base}_{timestamp}_{secrets.token_hex(3)}{ext}"
                )

                # Open new file first
                os.makedirs(os.path.dirname(self.dump_file_path), exist_ok=True)
                new_file = await aiofiles.open(self.dump_file_path, mode="wb")

                # Only close old file after new one opens successfully
                if old_file:
                    await old_file.close()

                self._file = new_file
            except Exception as e:
                # Keep old file handle if rotation fails
                self._file = old_file
                raise RuntimeError(f"Failed to rotate dump file: {e}") from e

    async def push_bytes(self, data: bytes):
        if not self._file:
            raise RuntimeError(
                f"Dumper for {self.dump_file_path} is not opened. Please start the Dumper first."
            )
        _ = await self._file.write(data)
