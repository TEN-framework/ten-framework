import asyncio
import os
from typing import Optional


class Dumper:
    """Audio dumper for debugging and analysis purposes."""
    
    def __init__(self, file_path: str):
        self.file_path = file_path
        self.file_handle: Optional[asyncio.StreamWriter] = None
        self.is_running = False

    async def start(self) -> None:
        """Start the audio dumper."""
        try:
            # Ensure directory exists
            os.makedirs(os.path.dirname(self.file_path), exist_ok=True)
            
            # Open file for binary writing
            self.file_handle = await asyncio.to_thread(open, self.file_path, 'wb')
            self.is_running = True
        except Exception as e:
            print(f"Failed to start audio dumper: {e}")
            self.is_running = False

    async def stop(self) -> None:
        """Stop the audio dumper."""
        if self.file_handle:
            try:
                await asyncio.to_thread(self.file_handle.close)
            except Exception as e:
                print(f"Error closing audio dump file: {e}")
            finally:
                self.file_handle = None
                self.is_running = False

    async def push_bytes(self, data: bytes) -> None:
        """Push audio bytes to the dump file."""
        if self.is_running and self.file_handle:
            try:
                await asyncio.to_thread(self.file_handle.write, data)
                await asyncio.to_thread(self.file_handle.flush)
            except Exception as e:
                print(f"Error writing to audio dump file: {e}")

    def __del__(self):
        """Cleanup when object is destroyed."""
        if self.file_handle:
            try:
                self.file_handle.close()
            except:
                pass
