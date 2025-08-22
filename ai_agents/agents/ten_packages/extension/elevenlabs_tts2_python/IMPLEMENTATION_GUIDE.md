# ElevenLabs TTS Implementation Guide

## Overview

This document describes the current implementation of the ElevenLabs TTS WebSocket client, which has been refactored to use a unified connection management pattern inspired by the ByteDance TTS implementation. The design addresses WebSocket concurrency issues, connection stability, and provides robust flush functionality.

## Architecture

### Core Design Principles

1. **Unified Connection Loop**: A single main loop manages all WebSocket connection lifecycle
2. **Automatic Reconnection**: Built-in reconnection mechanism using `async for websockets.connect()`
3. **Request-based Reconnection**: External components can request reconnection by signaling the main loop
4. **Immediate Flush Response**: Flush requests immediately disconnect and re-establish connections
5. **Concurrency Safety**: Lock mechanisms prevent race conditions

### Key Components

#### 1. Main Connection Loop (`_main_connection_loop`)

```python
async def _main_connection_loop(self):
    """Main connection loop that handles reconnection automatically"""
    while not self._session_closing:
        try:
            # Use websockets.connect infinite loop for automatic reconnection
            async for ws in websockets.connect(self.uri, ...):
                # Check if session closing is requested
                if self._session_closing:
                    break
                
                # Start send and receive tasks
                self._channel_tasks = [
                    asyncio.create_task(self._ws_recv_loop(ws)),
                    asyncio.create_task(self._ws_send_loop(ws))
                ]
                
                # Wait for tasks to complete or error
                await self._await_channel_tasks()
                
                # Check if connection was closed due to flush request
                if self._flush_requested:
                    self._flush_requested = False
                    continue  # Continue loop to re-establish connection
                
                # If we reach here, connection was closed and needs reconnection
                if not self._session_closing:
                    await self._handle_connection_loss()
```

#### 2. State Management

```python
class ElevenLabsTTS2:
    def __init__(self, ...):
        # New: Unified state management
        self._session_closing = False
        self._connection_lock = asyncio.Lock()
        self._reconnect_requested = False
        self._flush_requested = False  # New: flush request flag
        self._main_loop_task = None
        self._channel_tasks = []
```

#### 3. Channel Task Management

```python
async def _await_channel_tasks(self):
    """Wait for channel tasks to complete or error"""
    if not self._channel_tasks:
        return
        
    try:
        done, pending = await asyncio.wait(
            self._channel_tasks,
            return_when=asyncio.FIRST_EXCEPTION
        )
        
        # Cancel remaining tasks
        for task in pending:
            task.cancel()
            
        # Check for exceptions
        for task in done:
            exp = task.exception()
            if exp and not isinstance(exp, asyncio.CancelledError):
                raise exp
                
        # Check if tasks were cancelled due to flush request
        if self._flush_requested:
            self.ten_env.log_info("Flush detected in channel tasks - tasks were cancelled intentionally")
                
    except asyncio.CancelledError:
        # Main task cancelled, cancel all child tasks
        for task in self._channel_tasks:
            task.cancel()
        raise
    finally:
        self._channel_tasks.clear()
```

#### 4. Request-based Reconnection

```python
async def request_reconnect(self):
    """Request reconnection - set flag to let main loop handle reconnection"""
    async with self._connection_lock:
        if not self._reconnect_requested:
            self._reconnect_requested = True
            self.ten_env.log_info("Reconnect requested")
            
            # Trigger reconnection: close current connection
            if self.ws and self.ws.state.name != "CLOSED":
                try:
                    await self.ws.close()
                except Exception as e:
                    self.ten_env.log_error(f"Error closing WebSocket for reconnect: {e}")
```

#### 5. Flush Functionality

```python
async def handle_flush(self):
    """Handle flush request - immediately disconnect and re-establish connection"""
    try:
        self.ten_env.log_info("Flush requested - immediately disconnecting current connection")
        
        # Set flush flag
        self._flush_requested = True
        
        # Clear queues
        while not self.audio_data_queue.empty():
            try:
                self.audio_data_queue.get_nowait()
            except QueueEmpty:
                break

        while not self.text_input_queue.empty():
            try:
                self.text_input_queue.get_nowait()
            except QueueEmpty:
                break

        # Immediately close current WebSocket connection
        if self.ws and self.ws.state.name != "CLOSED":
            try:
                await self.ws.close()
                self.ten_env.log_info("Current WebSocket connection closed for flush")
            except Exception as e:
                self.ten_env.log_error(f"Error closing WebSocket for flush: {e}")

        # Reset connection state
        self.is_connected = False
        
        # Cancel current channel tasks
        for task in self._channel_tasks:
            if not task.done():
                task.cancel()
        
        # Wait for tasks to complete
        try:
            await asyncio.wait(self._channel_tasks, timeout=2.0)
        except asyncio.TimeoutError:
            self.ten_env.log_warning("Timeout waiting for channel tasks to cancel during flush")
        
        self._channel_tasks.clear()
        
        # Reset flush flag to let main loop re-establish connection
        self._flush_requested = False
        
        self.ten_env.log_info("Flush handling completed - connection will be re-established by main loop")

    except Exception as e:
        self.ten_env.log_error(f"Error handling flush: {e}")
        raise
```

## Key Features

### 1. Automatic Reconnection

The implementation uses `async for websockets.connect()` which automatically handles reconnection when the connection is lost. This eliminates the need for manual reconnection logic.

### 2. Unified State Management

All connection state is managed in one place through the main loop, preventing state inconsistencies and race conditions.

### 3. Immediate Flush Response

When a flush request is received:
1. The current WebSocket connection is immediately closed
2. All audio and text queues are cleared
3. Channel tasks are cancelled
4. The main loop automatically re-establishes the connection

### 4. Concurrency Safety

- `_connection_lock` prevents concurrent connection attempts
- State flags ensure proper synchronization
- Task management prevents resource leaks

### 5. Error Handling

- Exceptions in channel tasks are propagated to the main loop
- Graceful handling of cancellation
- Proper resource cleanup on errors

## Usage

### Initialization

```python
client = ElevenLabsTTS2(config, ten_env, error_callback)
await client.start_connection()  # Start the main connection loop
```

### Sending Text

```python
await client.text_input_queue.put(text_input)
```

### Getting Audio

```python
audio_data = await client.get_synthesized_audio()
```

### Requesting Reconnection

```python
await client.request_reconnect()  # Let main loop handle reconnection
```

### Flushing Audio

```python
await client.handle_flush()  # Immediately stop current audio and restart
```

### Closing Connection

```python
await client.close_connection()  # Graceful shutdown
```

## Error Handling

### Common Issues and Solutions

1. **WebSocket Concurrency Errors**: Resolved by unified connection management
2. **Task Cancellation Errors**: Proper exception handling in task cancellation
3. **Duplicate Connections**: Prevented by connection locks
4. **State Inconsistencies**: Eliminated by centralized state management
5. **Connection Blocking**: Resolved by adding timeouts to WebSocket operations
6. **Queue Blocking**: Resolved by adding timeouts to queue operations

### Recent Fixes

#### Connection Health Check Improvement
- **Problem**: Connection health check was too strict, requiring both WebSocket connection and main loop to be active
- **Solution**: Modified `is_connection_healthy()` to consider main loop running as healthy, even if WebSocket is still connecting
- **Impact**: Prevents unnecessary connection restart attempts

#### Timeout Protection
- **Problem**: WebSocket receive and send operations could block indefinitely
- **Solution**: Added 30-second timeouts to `ws.recv()` and `text_input_queue.get()` operations
- **Impact**: Prevents indefinite blocking and improves responsiveness

#### Enhanced Logging
- **Problem**: Limited visibility into connection state and task lifecycle
- **Solution**: Added comprehensive debug logging for connection establishment, task startup, and queue operations
- **Impact**: Better debugging and monitoring capabilities

### Logging

The implementation provides comprehensive logging for debugging:
- Connection establishment and closure
- Task lifecycle events
- Flush operations
- Error conditions

## Performance Considerations

1. **Memory Management**: Proper cleanup of tasks and queues
2. **Connection Efficiency**: Automatic reconnection without manual intervention
3. **Resource Usage**: Minimal overhead with unified loop design
4. **Latency**: Immediate response to flush requests

## Testing

### Recommended Test Scenarios

1. **Network Disconnection**: Test automatic reconnection
2. **Server-initiated Disconnection**: Verify graceful handling
3. **Concurrent Requests**: Ensure thread safety
4. **Flush Operations**: Verify immediate response
5. **Long-running Stability**: Test for memory leaks

### Test Commands

```bash
# Test basic functionality
python -m pytest tests/test_elevenlabs_tts.py

# Test with network simulation
python -m pytest tests/test_network_conditions.py

# Test flush functionality
python -m pytest tests/test_flush_operations.py
```

## Migration from Previous Implementation

### Breaking Changes

1. **Method Renames**: 
   - `text_to_speech_ws_streaming()` → `_ws_send_loop()`
   - `ws_recv_loop()` → `_ws_recv_loop()`

2. **Task Management**: No longer need to manually manage `ws_send_task` and `ws_recv_task`

3. **Connection Health**: Use `is_connection_healthy()` instead of checking individual tasks

### Migration Steps

1. Update import statements
2. Remove manual task management code
3. Use new connection health checking
4. Update error handling to use new patterns

## Future Enhancements

1. **Connection Pooling**: Support for multiple concurrent connections
2. **Advanced Retry Logic**: Configurable retry strategies
3. **Metrics Collection**: Performance monitoring and analytics
4. **Configuration Management**: Dynamic configuration updates

## Conclusion

This implementation provides a robust, scalable solution for ElevenLabs TTS WebSocket communication. The unified design pattern ensures stability, maintainability, and performance while providing immediate response to user requests like flush operations.

The architecture is inspired by proven patterns from ByteDance TTS and adapted specifically for ElevenLabs requirements, resulting in a production-ready solution.
