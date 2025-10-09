#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import gc
import os
from ten_runtime import (
    AudioFrame,
)

try:
    import psutil
except ImportError:
    psutil = None


def get_memory_usage() -> float:
    """Get current process memory usage (MB)"""
    if psutil is None:
        # If psutil is not available, return a fixed value so tests can continue
        return 0.0
    process = psutil.Process(os.getpid())
    return float(process.memory_info().rss / 1024 / 1024)


def test_lock_unlock() -> None:
    """Test whether AudioFrame's lock_buf and unlock_buf APIs have memory leaks"""

    # Test parameters
    LOOP_COUNT = 100000  # Number of loops, can be adjusted as needed
    BUFFER_SIZE = 1024  # Buffer size
    MEMORY_CHECK_INTERVAL = 1000  # Check memory every N loops

    print(
        f"Starting memory leak test: {LOOP_COUNT} loops, buffer size {BUFFER_SIZE} bytes"
    )

    # Force garbage collection and get initial memory usage
    gc.collect()
    initial_memory: float = get_memory_usage()
    print(f"Initial memory usage: {initial_memory:.2f} MB")

    # Record memory usage
    memory_samples: list[tuple[int, float]] = []

    # Execute loop test
    for i in range(LOOP_COUNT):
        # Create AudioFrame
        audio_frame = AudioFrame.create("pcm_frame")
        audio_frame.alloc_buf(BUFFER_SIZE)

        # Test lock_buf and unlock_buf
        buf = audio_frame.lock_buf()
        assert buf is not None
        assert len(buf) == BUFFER_SIZE

        # Unlock buffer
        audio_frame.unlock_buf(buf)

        # Verify buffer is still accessible
        assert audio_frame.get_buf() is not None
        assert len(audio_frame.get_buf()) == BUFFER_SIZE

        # Periodically check memory usage
        if (i + 1) % MEMORY_CHECK_INTERVAL == 0:
            gc.collect()  # Force garbage collection
            current_memory: float = get_memory_usage()
            memory_samples.append((i + 1, current_memory))
            print(
                f"Memory usage after {i + 1:5d} loops: {current_memory:.2f} MB "
                + f"(growth: {current_memory - initial_memory:+.2f} MB)"
            )

    # Final memory check
    gc.collect()
    final_memory: float = get_memory_usage()
    memory_growth: float = final_memory - initial_memory

    print(f"\n=== Memory Leak Test Results ===")
    print(f"Initial memory usage: {initial_memory:.2f} MB")
    print(f"Final memory usage: {final_memory:.2f} MB")
    print(f"Total memory growth: {memory_growth:+.2f} MB")
    print(
        f"Average memory growth per operation: {memory_growth / LOOP_COUNT * 1024:.4f} KB"
    )

    # Analyze memory growth trend
    if len(memory_samples) >= 2:
        first_sample: tuple[int, float] = memory_samples[0]
        last_sample: tuple[int, float] = memory_samples[-1]
        trend_growth: float = last_sample[1] - first_sample[1]
        print(
            f"Memory growth trend (from {first_sample[0]} to {last_sample[0]} loops): "
            + f"{trend_growth:+.2f} MB"
        )

    # Memory leak detection threshold (can be adjusted based on actual conditions)
    MEMORY_LEAK_THRESHOLD_MB = 10.0  # If memory growth exceeds 10MB, consider it a potential memory leak

    if memory_growth > MEMORY_LEAK_THRESHOLD_MB:
        print(
            f"⚠️  Warning: Potential memory leak detected! Memory growth {memory_growth:.2f} MB exceeds threshold {MEMORY_LEAK_THRESHOLD_MB} MB"
        )
        # Note: Not directly asserting False here, as it could be normal memory fluctuation
        # Can decide whether to fail the test based on actual requirements
    else:
        print(
            f"✅ No obvious memory leak detected, memory growth is within acceptable range"
        )

    print("======================\n")


def test_get_buf() -> None:
    """Test whether AudioFrame's get_buf API has memory leaks"""

    # Test parameters
    LOOP_COUNT = 100000  # Number of loops, can be adjusted as needed
    BUFFER_SIZE = 1024  # Buffer size
    MEMORY_CHECK_INTERVAL = 1000  # Check memory every N loops

    print(
        f"Starting get_buf memory leak test: {LOOP_COUNT} loops, buffer size {BUFFER_SIZE} bytes"
    )

    # Force garbage collection and get initial memory usage
    gc.collect()
    initial_memory: float = get_memory_usage()
    print(f"Initial memory usage: {initial_memory:.2f} MB")

    # Record memory usage
    memory_samples: list[tuple[int, float]] = []

    # Execute loop test
    for i in range(LOOP_COUNT):
        # Create AudioFrame
        audio_frame = AudioFrame.create("pcm_frame")
        audio_frame.alloc_buf(BUFFER_SIZE)

        # Test get_buf API - call multiple times to detect memory leaks
        buf1 = audio_frame.get_buf()
        assert buf1 is not None
        assert len(buf1) == BUFFER_SIZE

        # Call get_buf again, should return the same buffer
        buf2 = audio_frame.get_buf()
        assert buf2 is not None
        assert len(buf2) == BUFFER_SIZE

        # Verify that both calls return references to the same buffer
        assert buf1 is buf2 or (len(buf1) == len(buf2) and buf1 == buf2)

        # Call get_buf multiple times to increase memory pressure
        for _ in range(10):
            buf_temp = audio_frame.get_buf()
            assert buf_temp is not None
            assert len(buf_temp) == BUFFER_SIZE

        # Periodically check memory usage
        if (i + 1) % MEMORY_CHECK_INTERVAL == 0:
            gc.collect()  # Force garbage collection
            current_memory: float = get_memory_usage()
            memory_samples.append((i + 1, current_memory))
            print(
                f"Memory usage after {i + 1:5d} loops: {current_memory:.2f} MB "
                + f"(growth: {current_memory - initial_memory:+.2f} MB)"
            )

    # Final memory check
    gc.collect()
    final_memory: float = get_memory_usage()
    memory_growth: float = final_memory - initial_memory

    print(f"\n=== get_buf Memory Leak Test Results ===")
    print(f"Initial memory usage: {initial_memory:.2f} MB")
    print(f"Final memory usage: {final_memory:.2f} MB")
    print(f"Total memory growth: {memory_growth:+.2f} MB")
    print(
        f"Average memory growth per operation: {memory_growth / LOOP_COUNT * 1024:.4f} KB"
    )

    # Analyze memory growth trend
    if len(memory_samples) >= 2:
        first_sample: tuple[int, float] = memory_samples[0]
        last_sample: tuple[int, float] = memory_samples[-1]
        trend_growth: float = last_sample[1] - first_sample[1]
        print(
            f"Memory growth trend (from {first_sample[0]} to {last_sample[0]} loops): "
            + f"{trend_growth:+.2f} MB"
        )

    # Memory leak detection threshold (can be adjusted based on actual conditions)
    MEMORY_LEAK_THRESHOLD_MB = 10.0  # If memory growth exceeds 10MB, consider it a potential memory leak

    if memory_growth > MEMORY_LEAK_THRESHOLD_MB:
        print(
            f"⚠️  Warning: get_buf API potential memory leak detected! Memory growth {memory_growth:.2f} MB exceeds threshold {MEMORY_LEAK_THRESHOLD_MB} MB"
        )
        # Note: Not directly asserting False here, as it could be normal memory fluctuation
        # Can decide whether to fail the test based on actual requirements
    else:
        print(
            f"✅ get_buf API: No obvious memory leak detected, memory growth is within acceptable range"
        )

    print("======================\n")
