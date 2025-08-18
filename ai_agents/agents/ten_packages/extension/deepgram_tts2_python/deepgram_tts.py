import asyncio
import json
import websockets
import aiohttp
import time
from datetime import datetime
from typing import AsyncIterator, Optional, Dict, Any
from ten_runtime.async_ten_env import AsyncTenEnv
from pydantic import BaseModel, Field, ConfigDict
import uuid
import threading
from asyncio import Queue


class DeepgramTTSConfig(BaseModel):
    """Configuration for Deepgram TTS using BaseModel with parameter transparency"""
    api_key: str = ""
    model: str = "aura-asteria-en"
    voice: str = "aura-asteria-en"
    encoding: str = "linear16"
    sample_rate: int = 24000
    container: str = "none"
    # Connection options
    websocket_timeout: float = 10.0
    min_audio_threshold: int = 1000
    # Persistent connection options
    reconnect_attempts: int = 5
    reconnect_delay: float = 1.0
    keepalive_interval: float = 30.0
    max_request_retries: int = 2
    health_check_timeout: float = 5.0
    # Dump options
    dump_enabled: bool = False
    dump_path: str = "/tmp/tts_test_dump"
    
    # Parameter transparency - allows arbitrary parameters to be passed through
    params: Dict[str, Any] = Field(default_factory=dict)
    
    # Modern Pydantic v2 configuration
    model_config = ConfigDict(extra="allow", populate_by_name=True)
    
    def update_params(self) -> None:
        """Update config attributes from params dictionary for parameter transparency."""
        param_names = [
            "api_key",
            "model", 
            "voice",
            "encoding",
            "sample_rate",
            "container",
            "websocket_timeout",
            "min_audio_threshold",
            "reconnect_attempts",
            "reconnect_delay",
            "keepalive_interval",
            "max_request_retries",
            "health_check_timeout",
            "dump_enabled",
            "dump_path"
        ]
        
        for param_name in param_names:
            if param_name in self.params:
                setattr(self, param_name, self.params[param_name])


class StreamingTTSRequest:
    """Represents a streaming TTS request that accumulates text chunks"""
    def __init__(self, request_id: str):
        self.request_id = request_id
        self.text_chunks = []
        self.is_complete = False
        self.is_finalized = False  # Track if streaming chunks have been sent
        self.audio_started = False
        self.start_time = time.time()
        self.first_audio_time = None
        self.audio_queue = None  # Will be set when streaming starts
        self.completed = False
        self.error = None
        
    def add_text_chunk(self, text: str, is_end: bool = False):
        """Add a text chunk to the streaming request"""
        self.text_chunks.append(text)
        if is_end:
            self.is_complete = True
    
    def get_combined_text(self) -> str:
        """Get all text chunks combined"""
        return "".join(self.text_chunks)
    
    def mark_audio_started(self):
        """Mark when first audio chunk is received"""
        if not self.audio_started:
            self.audio_started = True
            self.first_audio_time = time.time()
    
    def get_ttfb_ms(self) -> int:
        """Get time to first byte in milliseconds"""
        if self.first_audio_time:
            return int((self.first_audio_time - self.start_time) * 1000)
        return 0


class DeepgramTTS:
    def __init__(self, config: DeepgramTTSConfig):
        self.config = config
        self.websocket_url = self._build_websocket_url()
        self.rest_url = self._build_rest_url()
        self.headers = {
            "Authorization": f"Token {config.api_key}"
        }
        
        # Lazy connection - don't connect during init
        self.websocket: Optional[websockets.WebSocketServerProtocol] = None
        self.connection_lock = asyncio.Lock()
        self.is_connected = False
        self.is_connecting = False  # Add missing attribute
        self.message_handler_task: Optional[asyncio.Task] = None
        
        # Streaming request management
        self.active_requests: Dict[str, StreamingTTSRequest] = {}
        self.ten_env: Optional[AsyncTenEnv] = None
        
        # Connection health
        self.keepalive_task: Optional[asyncio.Task] = None

    def _build_websocket_url(self) -> str:
        """Build the WebSocket URL with query parameters"""
        base_url = "wss://api.deepgram.com/v1/speak"
        params = [
            f"model={self.config.model}",
            f"encoding={self.config.encoding}",
            f"sample_rate={self.config.sample_rate}",
            f"container={self.config.container}"
        ]
        return f"{base_url}?{'&'.join(params)}"
    
    def _build_rest_url(self) -> str:
        """Build the REST API URL with query parameters"""
        base_url = "https://api.deepgram.com/v1/speak"
        params = [
            f"model={self.config.model}",
            f"encoding={self.config.encoding}",
            f"sample_rate={self.config.sample_rate}"
        ]
        return f"{base_url}?{'&'.join(params)}"

    async def initialize(self, ten_env: AsyncTenEnv) -> None:
        """Initialize the persistent WebSocket connection"""
        self.ten_env = ten_env
        ten_env.log_info("Initializing persistent Deepgram TTS WebSocket connection...")
        
        # Start the persistent connection
        await self._ensure_connection()
        
        # Start keepalive task
        self.keepalive_task = asyncio.create_task(self._keepalive_loop())
        
        ten_env.log_info("Deepgram TTS persistent connection initialized successfully")

    async def cleanup(self) -> None:
        """Clean up the persistent connection and tasks"""
        if self.ten_env:
            self.ten_env.log_info("Cleaning up Deepgram TTS persistent connection...")
        
        # Cancel keepalive task
        if self.keepalive_task:
            self.keepalive_task.cancel()
            try:
                await self.keepalive_task
            except asyncio.CancelledError:
                pass
        
        # Cancel message handler task
        if self.message_handler_task:
            self.message_handler_task.cancel()
            try:
                await self.message_handler_task
            except asyncio.CancelledError:
                pass
        
        # Close WebSocket connection
        if self.websocket:
            try:
                await self.websocket.close()
            except:
                pass
            self.websocket = None
        
        self.is_connected = False
        
        # Complete any pending requests with error
        for request in self.active_requests.values():
            if not request.completed:
                request.error = Exception("Connection closed")
                request.completed = True
                await request.audio_queue.put(None)  # Signal completion
        
        self.active_requests.clear()
        
        if self.ten_env:
            self.ten_env.log_info("Deepgram TTS cleanup completed")

    async def _ensure_connection(self) -> bool:
        """Ensure WebSocket connection is established with health check"""
        async with self.connection_lock:
            if self.ten_env:
                self.ten_env.log_info("DEBUG: _ensure_connection called")
            
            # Check if connection is truly healthy
            if self.is_connected and self.websocket:
                if self.ten_env:
                    self.ten_env.log_info("DEBUG: Connection exists, performing health check")
                # Perform health check
                if await self._health_check():
                    if self.ten_env:
                        self.ten_env.log_info("DEBUG: Health check passed")
                    return True
                else:
                    if self.ten_env:
                        self.ten_env.log_warn("Connection health check failed, reconnecting...")
                    self.is_connected = False
            
            if self.is_connecting:
                if self.ten_env:
                    self.ten_env.log_info("DEBUG: Already connecting, waiting...")
                # Wait for ongoing connection attempt
                while self.is_connecting:
                    await asyncio.sleep(0.1)
                return self.is_connected
            
            return await self._connect()

    async def _health_check(self) -> bool:
        """Perform a health check on the WebSocket connection"""
        try:
            if not self.websocket:
                return False
            
            # Send a ping to check connection health
            pong_waiter = await self.websocket.ping()
            await asyncio.wait_for(pong_waiter, timeout=5.0)
            return True
            
        except (websockets.exceptions.ConnectionClosed, 
                websockets.exceptions.WebSocketException,
                asyncio.TimeoutError,
                Exception) as e:
            if self.ten_env:
                self.ten_env.log_warn(f"Health check failed: {str(e)}")
            return False

    async def _connect(self) -> bool:
        """Establish WebSocket connection with exponential backoff retry logic"""
        self.is_connecting = True
        
        for attempt in range(self.config.reconnect_attempts):
            try:
                if self.ten_env:
                    self.ten_env.log_info(f"Connecting to Deepgram TTS WebSocket (attempt {attempt + 1}/{self.config.reconnect_attempts})")
                
                # Close existing connection if any
                if self.websocket:
                    try:
                        await self.websocket.close()
                    except:
                        pass
                
                # Create new connection with timeout
                self.websocket = await asyncio.wait_for(
                    websockets.connect(
                        self.websocket_url,
                        additional_headers=self.headers,
                        ping_interval=20,
                        ping_timeout=10,
                        close_timeout=10,
                        max_size=None,  # Allow large audio frames
                        compression=None  # Disable compression for audio
                    ),
                    timeout=15.0  # Connection timeout
                )
                
                self.is_connected = True
                self.is_connecting = False
                self.last_ping_time = time.time()
                
                # Start message handler
                if self.message_handler_task:
                    self.message_handler_task.cancel()
                self.message_handler_task = asyncio.create_task(self._handle_messages())
                
                if self.ten_env:
                    self.ten_env.log_info("Successfully connected to Deepgram TTS WebSocket")
                
                return True
                
            except (websockets.exceptions.WebSocketException,
                    websockets.exceptions.InvalidURI,
                    websockets.exceptions.InvalidHandshake,
                    asyncio.TimeoutError,
                    ConnectionRefusedError,
                    OSError) as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Connection attempt {attempt + 1} failed: {str(e)}")
                # Check for authentication errors (HTTP 401) - do not retry
                if "HTTP 401" in str(e):
                    if self.ten_env:
                        self.ten_env.log_error("Authentication failed: Invalid API key")
                    raise Exception("Invalid Deepgram API key")
                
                # Exponential backoff with jitter
                if attempt < self.config.reconnect_attempts - 1:
                    backoff_time = self.config.reconnect_delay * (2 ** attempt) + (time.time() % 1)
                    if self.ten_env:
                        self.ten_env.log_info(f"Retrying in {backoff_time:.2f} seconds...")
                    await asyncio.sleep(backoff_time)
                else:
                    self.is_connecting = False
                    if self.ten_env:
                        self.ten_env.log_error("All connection attempts failed")
                    return False
            
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Unexpected connection error: {str(e)}")
                if attempt < self.config.reconnect_attempts - 1:
                    await asyncio.sleep(self.config.reconnect_delay * (attempt + 1))
                else:
                    self.is_connecting = False
                    return False
        
        self.is_connecting = False
        return False

    async def _handle_messages(self) -> None:
        """Handle incoming WebSocket messages with robust error handling"""
        try:
            async for message in self.websocket:
                if isinstance(message, bytes):
                    # This is audio data - route to appropriate request
                    await self._handle_audio_data(message)
                else:
                    # This is a text message (metadata)
                    await self._handle_text_message(message)
                    
        except websockets.exceptions.ConnectionClosed as e:
            if self.ten_env:
                self.ten_env.log_warn(f"WebSocket connection closed: {e}")
            self.is_connected = False
            await self._handle_connection_loss()
            
        except websockets.exceptions.WebSocketException as e:
            if self.ten_env:
                self.ten_env.log_error(f"WebSocket error: {str(e)}")
            self.is_connected = False
            await self._handle_connection_loss()
            
        except asyncio.CancelledError:
            if self.ten_env:
                self.ten_env.log_debug("Message handler cancelled")
            raise
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Unexpected error handling WebSocket messages: {str(e)}")
            self.is_connected = False
            await self._handle_connection_loss()

    async def _handle_connection_loss(self) -> None:
        """Handle connection loss and attempt reconnection"""
        if self.ten_env:
            self.ten_env.log_warn("Connection lost, attempting to reconnect...")
        
        # Mark all pending requests as potentially failed
        for request in self.active_requests.values():
            if not request.completed:
                # Don't mark as error yet, give reconnection a chance
                if self.ten_env:
                    self.ten_env.log_debug(f"Request {request.request_id} affected by connection loss")
        
        # Attempt to reconnect in background
        asyncio.create_task(self._background_reconnect())

    async def _handle_audio_data(self, audio_data: bytes) -> None:
        """Handle incoming audio data and route to appropriate request"""
        if self.ten_env:
            self.ten_env.log_debug(f"Received audio chunk of size: {len(audio_data)} bytes")
        
        # Route to active streaming requests
        if self.active_requests:
            # Get the most recent active request (in a real implementation, 
            # you'd need proper request correlation)
            latest_request = list(self.active_requests.values())[-1]
            if latest_request.audio_queue:
                await latest_request.audio_queue.put(audio_data)
            else:
                if self.ten_env:
                    self.ten_env.log_warn(f"No audio queue for request {latest_request.request_id}")

    async def _handle_text_message(self, message: str) -> None:
        """Handle incoming text messages (metadata, errors, etc.)"""
        try:
            data = json.loads(message)
            message_type = data.get("type", "")
            
            if self.ten_env:
                self.ten_env.log_debug(f"Received WebSocket message: {message_type}")
            
            if message_type == "Metadata":
                if self.ten_env:
                    self.ten_env.log_debug(f"Received metadata: {data}")
            elif message_type == "Flushed":
                # Signal completion for active requests
                if self.active_requests:
                    latest_request = list(self.active_requests.values())[-1]
                    if latest_request.audio_queue:
                        await latest_request.audio_queue.put(None)  # Signal completion
                if self.ten_env:
                    self.ten_env.log_info("Audio stream flushed - request complete")
            elif message_type == "Error":
                error_msg = data.get("error", "Unknown error")
                if self.ten_env:
                    self.ten_env.log_error(f"Deepgram TTS error: {error_msg}")
                # Signal error for all active requests
                for request in self.active_requests.values():
                    if request.audio_queue:
                        await request.audio_queue.put(None)  # Signal completion with error
                        request.error = Exception(f"Deepgram error: {error_msg}")
                        request.completed = True
                        await request.audio_queue.put(None)
            else:
                if self.ten_env:
                    self.ten_env.log_debug(f"Received message: {data}")
                    
        except json.JSONDecodeError:
            if self.ten_env:
                self.ten_env.log_warn(f"Received non-JSON message: {message}")

    async def _background_reconnect(self) -> None:
        """Attempt reconnection in the background"""
        try:
            # Wait a bit before attempting reconnection
            await asyncio.sleep(1.0)
            
            if not self.is_connected and not self.is_connecting:
                success = await self._ensure_connection()
                if success and self.ten_env:
                    self.ten_env.log_info("Successfully reconnected to Deepgram WebSocket")
                elif self.ten_env:
                    self.ten_env.log_error("Failed to reconnect to Deepgram WebSocket")
                    
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Background reconnection failed: {str(e)}")

    async def _keepalive_loop(self) -> None:
        """Maintain connection health with periodic pings and reconnection"""
        consecutive_failures = 0
        max_consecutive_failures = 3
        
        while True:
            try:
                await asyncio.sleep(self.config.keepalive_interval)
                
                if self.is_connected and self.websocket and self.websocket:
                    # Perform health check
                    if await self._health_check():
                        consecutive_failures = 0
                        if self.ten_env:
                            self.ten_env.log_debug("Keepalive ping successful")
                    else:
                        consecutive_failures += 1
                        if self.ten_env:
                            self.ten_env.log_warn(f"Keepalive failed ({consecutive_failures}/{max_consecutive_failures})")
                        
                        if consecutive_failures >= max_consecutive_failures:
                            if self.ten_env:
                                self.ten_env.log_error("Multiple keepalive failures, marking connection as lost")
                            self.is_connected = False
                            await self._handle_connection_loss()
                            consecutive_failures = 0
                else:
                    # Connection is not healthy, try to reconnect
                    if self.ten_env:
                        self.ten_env.log_info("Connection not healthy, attempting to reconnect...")
                    await self._ensure_connection()
                    
            except asyncio.CancelledError:
                if self.ten_env:
                    self.ten_env.log_debug("Keepalive loop cancelled")
                break
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Keepalive error: {str(e)}")
                consecutive_failures += 1
                if consecutive_failures >= max_consecutive_failures:
                    self.is_connected = False

    async def add_text_chunk(self, request_id: str, text: str, is_end: bool = False) -> None:
        """Add a text chunk to a streaming request"""
        if request_id not in self.active_requests:
            # Create new streaming request with audio queue
            request = StreamingTTSRequest(request_id)
            # Set up audio queue immediately for streaming chunks
            request.audio_queue = asyncio.Queue()
            self.active_requests[request_id] = request
            if self.ten_env:
                self.ten_env.log_info(f"Created new streaming request with audio queue: {request_id}")
        
        request = self.active_requests[request_id]
        request.add_text_chunk(text, is_end)
        
        if self.ten_env:
            self.ten_env.log_info(f"Added text chunk to {request_id}: '{text[:30]}...' (is_end: {is_end})")
        
        # Process each chunk immediately for real-time streaming
        if text.strip():  # Process any chunk with actual content
            await self._process_streaming_chunk(request, text, is_end)
        
        # If this is the end, finalize the request
        if is_end:
            await self._finalize_streaming_request(request)
    
    async def _process_streaming_chunk(self, request: StreamingTTSRequest, text: str, is_final: bool = False) -> None:
        """Process a streaming chunk immediately for real-time audio"""
        try:
            if self.ten_env:
                self.ten_env.log_info(f"Processing streaming chunk for {request.request_id}: '{text[:30]}...' (is_final: {is_final})")
            
            # Ensure connection
            if not await self._ensure_connection():
                raise Exception("Failed to establish WebSocket connection")
            
            # Send this chunk to Deepgram and handle audio response
            await self._send_and_stream_chunk(request.request_id, text, is_final)
            
            # Wait briefly for audio response and stream it
            await self._stream_chunk_audio(request)
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error processing streaming chunk {request.request_id}: {str(e)}")
            raise

    async def _stream_chunk_audio(self, request: StreamingTTSRequest) -> None:
        """Stream audio for a single chunk"""
        try:
            # Determine timeout based on text length (longer text needs more time)
            text_length = len(request.get_combined_text())
            # Estimate: ~150 words per minute, ~5 chars per word = ~12.5 chars per second
            estimated_duration = max(text_length / 12.5, 2.0)  # At least 2 seconds
            timeout = min(estimated_duration * 2, 15.0)  # Max 15 seconds, double the estimate
            
            if self.ten_env:
                self.ten_env.log_info(f"Streaming audio for {text_length} chars, timeout: {timeout:.1f}s")
            
            start_time = time.time()
            
            while (time.time() - start_time) < timeout:
                try:
                    # Try to get audio chunk with short timeout
                    audio_chunk = await asyncio.wait_for(
                        request.audio_queue.get(), 
                        timeout=0.1
                    )
                    
                    if audio_chunk is None:
                        # End of audio for this chunk
                        if self.ten_env:
                            self.ten_env.log_info(f"Audio stream ended for chunk {request.request_id}")
                        break
                        
                    if len(audio_chunk) > 0:
                        # Send audio data immediately
                        if hasattr(self, 'extension_ref') and self.extension_ref:
                            await self.extension_ref.send_tts_audio_data(audio_chunk)
                        if self.ten_env:
                            self.ten_env.log_info(f"Streamed {len(audio_chunk)} bytes for chunk {request.request_id}")
                            
                except asyncio.TimeoutError:
                    # No audio available yet, continue waiting
                    continue
            
            elapsed_time = time.time() - start_time
            if self.ten_env:
                self.ten_env.log_info(f"Audio streaming completed for {request.request_id} in {elapsed_time:.1f}s")
                    
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error streaming chunk audio: {str(e)}")

    async def _send_and_stream_chunk(self, request_id: str, text: str, is_final: bool = False) -> None:
        """Send chunk to Deepgram and immediately start streaming the audio response"""
        try:
            # Send text message for this chunk
            text_message = {
                "type": "Speak",
                "text": text
            }
            await self.websocket.send(json.dumps(text_message))
            if self.ten_env:
                self.ten_env.log_info(f"Sent streaming chunk to Deepgram for request {request_id}: '{text[:30]}...'")
            
            # Send flush immediately to get audio for this chunk
            flush_message = {
                "type": "Flush"
            }
            await self.websocket.send(json.dumps(flush_message))
            if self.ten_env:
                self.ten_env.log_info(f"Sent flush for chunk {request_id} to get immediate audio")
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error sending streaming chunk to Deepgram: {str(e)}")
            raise

    async def _finalize_streaming_request(self, request: StreamingTTSRequest) -> None:
        """Finalize a streaming request"""
        try:
            if self.ten_env:
                self.ten_env.log_info(f"Finalizing streaming request {request.request_id}")
            
            # Mark the request as finalized - the extension will handle cleanup
            request.is_finalized = True
            
            # Don't delete the request here - let _start_streaming_audio handle it
                
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error finalizing streaming request {request.request_id}: {str(e)}")

    async def _process_complete_request(self, request: StreamingTTSRequest) -> None:
        """Process a complete streaming request"""
        try:
            combined_text = request.get_combined_text()
            if self.ten_env:
                self.ten_env.log_info(f"Processing complete request {request.request_id}: '{combined_text[:50]}...' (total length: {len(combined_text)})")
            
            # Ensure connection
            if not await self._ensure_connection():
                raise Exception("Failed to establish WebSocket connection")
            
            # Send the complete text to Deepgram
            await self._send_text_to_deepgram(request.request_id, combined_text)
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error processing complete request {request.request_id}: {str(e)}")
            # Clean up failed request
            if request.request_id in self.active_requests:
                del self.active_requests[request.request_id]
            raise
    
    async def _send_text_to_deepgram(self, request_id: str, text: str) -> None:
        """Send complete text to Deepgram WebSocket"""
        try:
            # Send text message
            text_message = {
                "type": "Speak",
                "text": text
            }
            await self.websocket.send(json.dumps(text_message))
            if self.ten_env:
                self.ten_env.log_info(f"Sent text to Deepgram for request {request_id}")
            
            # Send flush to trigger audio generation
            flush_command = {"type": "Flush"}
            await self.websocket.send(json.dumps(flush_command))
            if self.ten_env:
                self.ten_env.log_info(f"Sent flush command for request {request_id}")
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error sending text to Deepgram: {str(e)}")
            raise

    async def get_streaming_audio(self, request_id: str) -> AsyncIterator[bytes]:
        """Get streaming audio for a specific request"""
        if request_id not in self.active_requests:
            raise ValueError(f"Request {request_id} not found")
        
        request = self.active_requests[request_id]
        
        try:
            # Use existing audio queue or create one if needed
            if not request.audio_queue:
                audio_queue = asyncio.Queue()
                request.audio_queue = audio_queue
            else:
                audio_queue = request.audio_queue
            
            # Stream audio chunks as they arrive from WebSocket message handler
            chunk_count = 0
            request_timeout = self.config.websocket_timeout * 2
            
            while True:
                try:
                    # Wait for audio chunk with timeout
                    audio_chunk = await asyncio.wait_for(
                        audio_queue.get(), 
                        timeout=request_timeout
                    )
                    
                    # Check for end marker
                    if audio_chunk is None:
                        if self.ten_env:
                            self.ten_env.log_info(f"Received end marker for request {request_id}")
                        break
                    
                    chunk_count += 1
                    if self.ten_env:
                        self.ten_env.log_debug(f"Yielding audio chunk #{chunk_count} for {request_id}")
                    yield audio_chunk
                    
                except asyncio.TimeoutError:
                    if self.ten_env:
                        self.ten_env.log_error(f"Timeout waiting for audio chunks for request {request_id}")
                    break
                    
            if self.ten_env:
                self.ten_env.log_info(f"Audio streaming completed for {request_id}: {chunk_count} chunks")
            
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in streaming audio for {request_id}: {str(e)}")
            raise
        finally:
            # Clean up completed request
            if request_id in self.active_requests:
                del self.active_requests[request_id]

    async def get(self, ten_env: AsyncTenEnv, text: str) -> AsyncIterator[bytes]:
        """
        Get TTS audio using persistent WebSocket connection with robust error handling
        """
        request_id = str(uuid.uuid4())
        request = StreamingTTSRequest(request_id)
        max_retries = 2
        
        for retry_attempt in range(max_retries + 1):
            try:
                # Ensure connection is available
                if not await self._ensure_connection():
                    if self.ten_env:
                        self.ten_env.log_error("Failed to establish WebSocket connection")
                    if retry_attempt == max_retries:
                        if hasattr(self.config, 'use_rest_fallback') and self.config.use_rest_fallback:
                            if self.ten_env:
                                self.ten_env.log_info("All WebSocket attempts failed, falling back to REST API")
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        else:
                            raise Exception("WebSocket connection failed and REST API fallback disabled")
                    else:
                        if self.ten_env:
                            self.ten_env.log_info(f"Retrying WebSocket connection (attempt {retry_attempt + 1}/{max_retries + 1})")
                        await asyncio.sleep(1.0 * (retry_attempt + 1))
                        continue
                
                # Add request to active requests
                self.active_requests[request_id] = request
                
                if self.ten_env:
                    self.ten_env.log_info(f"Sending TTS request via persistent WebSocket: {text[:50]}... (attempt {retry_attempt + 1})")
                
                # Send the text to be synthesized
                message = {
                    "type": "Speak",
                    "text": text
                }
                
                await self.websocket.send(json.dumps(message))
                if self.ten_env:
                    self.ten_env.log_info(f"Sent text to Deepgram via persistent connection")
                
                # Send Flush command to trigger audio generation
                flush_command = {"type": "Flush"}
                await self.websocket.send(json.dumps(flush_command))
                if self.ten_env:
                    self.ten_env.log_info("Sent Flush command via persistent connection")
                
                # Create audio queue for this request
                audio_queue = asyncio.Queue()
                request.audio_queue = audio_queue
                
                # Stream audio chunks as they arrive
                chunk_count = 0
                request_timeout = self.config.websocket_timeout * 2
                
                while True:
                    try:
                        audio_chunk = await asyncio.wait_for(
                            audio_queue.get(),
                            timeout=request_timeout
                        )
                        
                        # Check for end marker
                        if audio_chunk is None:
                            break
                        
                        chunk_count += 1
                        if self.ten_env:
                            self.ten_env.log_debug(f"Yielding audio chunk #{chunk_count} of size: {len(audio_chunk)} bytes")
                        yield audio_chunk
                        
                    except asyncio.TimeoutError:
                        if self.ten_env:
                            self.ten_env.log_warn(f"Timeout waiting for audio chunk (request_id: {request_id}, chunk: {chunk_count})")
                        if chunk_count == 0:
                            # No audio received at all, this might be a connection issue
                            if not self.is_connected or not self.websocket:
                                if self.ten_env:
                                    self.ten_env.log_warn("Connection lost during request, will retry")
                                break
                        # If we got some chunks, continue waiting a bit more
                        if chunk_count > 0:
                            continue
                        else:
                            break
                
                # Clean up request
                if request_id in self.active_requests:
                    del self.active_requests[request_id]
                
                if chunk_count > 0:
                    if self.ten_env:
                        self.ten_env.log_info(f"TTS request completed successfully - yielded {chunk_count} chunks")
                    return  # Success, exit retry loop
                else:
                    if self.ten_env:
                        self.ten_env.log_warn(f"No audio chunks received for request {request_id}")
                    if retry_attempt < max_retries:
                        if self.ten_env:
                            self.ten_env.log_info("Retrying request...")
                        continue
                    else:
                        # All retries failed, fall back to REST API if enabled
                        if hasattr(self.config, 'use_rest_fallback') and self.config.use_rest_fallback:
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        else:
                            raise Exception("No audio received and REST API fallback disabled")
                        
            except (websockets.exceptions.ConnectionClosed,
                    websockets.exceptions.WebSocketException,
                    ConnectionResetError,
                    OSError) as e:
                if self.ten_env:
                    self.ten_env.log_error(f"WebSocket connection error (attempt {retry_attempt + 1}): {str(e)}")
                self.is_connected = False
                
                if retry_attempt < max_retries:
                    if self.ten_env:
                        self.ten_env.log_info("Connection error, retrying with new connection...")
                    await asyncio.sleep(1.0 * (retry_attempt + 1))
                    continue
                else:
                    # All retries failed, fall back to REST API
                    if hasattr(self.config, 'use_rest_fallback') and self.config.use_rest_fallback:
                        if self.ten_env:
                            self.ten_env.log_info("All WebSocket retries failed, falling back to REST API...")
                        try:
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        except Exception as rest_error:
                            if self.ten_env:
                                self.ten_env.log_error(f"REST API fallback error: {str(rest_error)}")
                            raise
                    else:
                        raise
                        
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Unexpected error in TTS request (attempt {retry_attempt + 1}): {str(e)}")
                if retry_attempt < max_retries:
                    await asyncio.sleep(1.0 * (retry_attempt + 1))
                    continue
                else:
                    # Final attempt failed
                    if hasattr(self.config, 'use_rest_fallback') and self.config.use_rest_fallback:
                        if self.ten_env:
                            self.ten_env.log_info("Falling back to REST API after unexpected error...")
                        try:
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        except Exception as rest_error:
                            if self.ten_env:
                                self.ten_env.log_error(f"REST API fallback error: {str(rest_error)}")
                            raise
                    else:
                        raise

    async def _get_rest_audio_streaming(self, ten_env: AsyncTenEnv, text: str) -> AsyncIterator[bytes]:
        """
        Get audio using REST API and stream it in chunks (fallback method)
        """
        try:
            if self.ten_env:
                self.ten_env.log_info(f"Using Deepgram REST API: {self.rest_url}")
            
            headers = {
                "Authorization": f"Token {self.config.api_key}",
                "Content-Type": "application/json"
            }
            
            data = {"text": text}
            
            async with aiohttp.ClientSession() as session:
                async with session.post(self.rest_url, headers=headers, json=data) as response:
                    if response.status == 200:
                        audio_data = await response.read()
                        if self.ten_env:
                            self.ten_env.log_info(f"REST API generated {len(audio_data)} bytes")
                        
                        # Stream the audio in chunks
                        chunk_size = 4096
                        for i in range(0, len(audio_data), chunk_size):
                            chunk = audio_data[i:i + chunk_size]
                            yield chunk
                    else:
                        error_text = await response.text()
                        if self.ten_env:
                            self.ten_env.log_error(f"REST API error {response.status}: {error_text}")
                        raise Exception(f"REST API error: {response.status} - {error_text}")
                        
        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error in Deepgram REST API: {str(e)}")
            raise
