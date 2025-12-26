#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import aiofiles
import os
from datetime import datetime


class Dumper:
    def __init__(self, dump_file_path: str):
        self.dump_file_base_path: str = dump_file_path
        self.dump_file_path: str = dump_file_path
        self._file: aiofiles.threadpool.binary.AsyncBufferedIOBase | None = None

    async def start(self):
        if self._file:
            return

        os.makedirs(os.path.dirname(self.dump_file_path), exist_ok=True)

        self._file = await aiofiles.open(self.dump_file_path, mode="wb")

    async def stop(self):
        if self._file:
            await self._file.close()
            self._file = None

    async def rotate(self):
        """Close current file and open new one with timestamp suffix."""
        if self._file:
            await self._file.close()

        # Generate timestamped filename: soniox_asr_in_20250126_143052.pcm
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        base, ext = os.path.splitext(self.dump_file_base_path)
        self.dump_file_path = f"{base}_{timestamp}{ext}"

        os.makedirs(os.path.dirname(self.dump_file_path), exist_ok=True)
        self._file = await aiofiles.open(self.dump_file_path, mode="wb")

    async def push_bytes(self, data: bytes):
        if not self._file:
            raise RuntimeError(
                "Dumper for {} is not opened. Please start the Dumper first.".format(
                    self.dump_file_path
                )
            )
        _ = await self._file.write(data)
