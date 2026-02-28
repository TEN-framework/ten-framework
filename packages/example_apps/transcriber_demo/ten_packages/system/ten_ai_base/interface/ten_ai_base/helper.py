#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from collections import deque
from datetime import datetime
import functools
from typing import Callable, Optional
from ten_runtime.async_ten_env import AsyncTenEnv
import time


def get_property_bool(ten_env: AsyncTenEnv, property_name: str) -> bool:
    """Helper to get boolean property from ten_env with error handling."""
    try:
        return ten_env.get_property_bool(property_name)
    except Exception as err:
        ten_env.log_warn(f"GetProperty {property_name} failed: {err}")
        return False


def get_properties_bool(
    ten_env: AsyncTenEnv,
    property_names: list[str],
    callback: Callable[[str, bool], None],
) -> None:
    """Helper to get boolean properties from ten_env with error handling."""
    for property_name in property_names:
        callback(property_name, get_property_bool(ten_env, property_name))


def get_property_string(ten_env: AsyncTenEnv, property_name: str) -> str:
    """Helper to get string property from ten_env with error handling."""
    try:
        return ten_env.get_property_string(property_name)
    except Exception as err:
        ten_env.log_warn(f"GetProperty {property_name} failed: {err}")
        return ""


def get_properties_string(
    ten_env: AsyncTenEnv,
    property_names: list[str],
    callback: Callable[[str, str], None],
) -> None:
    """Helper to get string properties from ten_env with error handling."""
    for property_name in property_names:
        callback(property_name, get_property_string(ten_env, property_name))


def get_property_int(ten_env: AsyncTenEnv, property_name: str) -> int:
    """Helper to get int property from ten_env with error handling."""
    try:
        return ten_env.get_property_int(property_name)
    except Exception as err:
        ten_env.log_warn(f"GetProperty {property_name} failed: {err}")
        return 0


def get_properties_int(
    ten_env: AsyncTenEnv,
    property_names: list[str],
    callback: Callable[[str, int], None],
) -> None:
    """Helper to get int properties from ten_env with error handling."""
    for property_name in property_names:
        callback(property_name, get_property_int(ten_env, property_name))


def get_property_float(ten_env: AsyncTenEnv, property_name: str) -> float:
    """Helper to get float property from ten_env with error handling."""
    try:
        return ten_env.get_property_float(property_name)
    except Exception as err:
        ten_env.log_warn(f"GetProperty {property_name} failed: {err}")
        return 0.0


def get_properties_float(
    ten_env: AsyncTenEnv,
    property_names: list[str],
    callback: Callable[[str, float], None],
) -> None:
    """Helper to get float properties from ten_env with error handling."""
    for property_name in property_names:
        callback(property_name, get_property_float(ten_env, property_name))


class AsyncEventEmitter:
    def __init__(self):
        self.listeners = {}

    def on(self, event_name, listener):
        """Register an event listener."""
        if event_name not in self.listeners:
            self.listeners[event_name] = []
        self.listeners[event_name].append(listener)

    def emit(self, event_name, *args, **kwargs):
        """Fire the event without waiting for listeners to finish."""
        if event_name in self.listeners:
            for listener in self.listeners[event_name]:
                asyncio.create_task(listener(*args, **kwargs))


class AsyncQueue:
    def __init__(self):
        self._queue = deque()  # Use deque for efficient prepend and append
        self._condition = asyncio.Condition()  # Use Condition to manage access

    async def put(self, item, prepend=False):
        """Add an item to the queue (prepend if specified)."""
        async with self._condition:
            if prepend:
                self._queue.appendleft(item)  # Prepend item to the front
            else:
                self._queue.append(item)  # Append item to the back
            self._condition.notify()

    async def get(self):
        """Remove and return an item from the queue."""
        async with self._condition:
            while not self._queue:
                await self._condition.wait()  # Wait until an item is available
            return self._queue.popleft()  # Pop from the front of the deque

    async def flush(self):
        """Flush all items from the queue."""
        async with self._condition:
            while self._queue:
                self._queue.popleft()  # Clear the queue
            self._condition.notify_all()  # Notify all consumers that the queue is empty

    def __len__(self):
        """Return the current size of the queue."""
        return len(self._queue)


def write_pcm_to_file(buffer: bytearray, file_name: str) -> None:
    """Helper function to write PCM data to a file."""
    with open(file_name, "ab") as f:  # append to file
        f.write(buffer)


def generate_file_name(prefix: str) -> str:
    # Create a timestamp for the file name
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"{prefix}_{timestamp}.pcm"

class PCMWriter:
    def __init__(self, file_path: str, buffer_size: int = 1024 * 64):
        self.file_name = file_path
        self.buffer = bytearray()
        self.buffer_size = buffer_size
        self._lock = asyncio.Lock()
        self._flush_task: Optional[asyncio.Task] = None

    async def write(self, data: bytes) -> None:
        async with self._lock:
            self.buffer.extend(data)
            if len(self.buffer) >= self.buffer_size:
                self._schedule_flush()

    async def flush(self) -> None:
        """Force flush remaining data to file and wait until done."""
        async with self._lock:
            self._schedule_flush(force=True)
        if self._flush_task:
            await self._flush_task

    def _schedule_flush(self, force: bool = False) -> None:
        if self._flush_task and not self._flush_task.done():
            return  # A flush is already running

        data_to_write = bytes(self.buffer)
        self.buffer.clear()

        if not data_to_write and not force:
            return

        loop = asyncio.get_running_loop()
        self._flush_task = loop.create_task(self._flush(data_to_write))

    async def _flush(self, data: bytes) -> None:
        if not data:
            return
        loop = asyncio.get_running_loop()
        await loop.run_in_executor(
            None, functools.partial(write_pcm_to_file, data, self.file_name)
        )



class TimeHelper:
    """
    A helper class for time calculations.

    This class provides a set of class methods to calculate the duration
    between two time points or from a given time point to the present.
    It supports returning durations in seconds or milliseconds.
    """

    @classmethod
    def duration(cls, start: float, end: float) -> float:
        """
        Calculate the duration between two time points.

        Args:
            start (float): The start time in seconds since the epoch.
            end (float): The end time in seconds since the epoch.

        Returns:
            float: The time difference between the two points in seconds.
        """

        return end - start

    @classmethod
    def duration_since(cls, start: float) -> float:
        """
        Calculate the duration from a given time point to now.

        Args:
            start (float): The start time in seconds since the epoch.

        Returns:
            float: The time difference from the start time to now in seconds.
        """

        return cls.duration(start, time.time())

    @classmethod
    def duration_ms(cls, start: float, end: float) -> int:
        """
        Calculate the duration between two time points in milliseconds.

        Args:
            start (float): The start time in seconds since the epoch.
            end (float): The end time in seconds since the epoch.

        Returns:
            int: The time difference between the two points in milliseconds.
        """

        return int((end - start) * 1000)

    @classmethod
    def duration_ms_since(cls, start: float) -> int:
        """
        Calculate the duration from a given time point to now in milliseconds.

        Args:
            start (float): The start time in seconds since the epoch.

        Returns:
            int: The time difference from the start time to now in milliseconds.
        """

        return cls.duration_ms(start, time.time())
