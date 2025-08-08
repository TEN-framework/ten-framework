import asyncio
import json
import websockets
import aiohttp
import time
from dataclasses import dataclass
from datetime import datetime
from typing import AsyncIterator, Optional, Dict, Any
from ten_runtime.async_ten_env import AsyncTenEnv
from ten_ai_base.config import BaseConfig
import uuid
import threading
from asyncio import Queue


@dataclass
class DeepgramTTSConfig(BaseConfig):
    api_key: str = ""
    model: str = "aura-asteria-en"
    voice: str = "aura-asteria-en"
    encoding: str = "linear16"
    sample_rate: int = 24000
    container: str = "none"
    # Enhanced options
    use_rest_fallback: bool = True
    websocket_timeout: float = 10.0
    min_audio_threshold: int = 1000
    # Persistent connection options
    reconnect_attempts: int = 5
    reconnect_delay: float = 1.0
    keepalive_interval: float = 30.0
    max_request_retries: int = 2
    health_check_timeout: float = 5.0


class TTSRequest:
    """Represents a TTS request with its associated response queue"""
    def __init__(self, request_id: str, text: str):
        self.request_id = request_id
        self.text = text
        self.audio_queue: Queue = Queue()
        self.completed = False
        self.error: Optional[Exception] = None


class DeepgramTTS:
    def __init__(self, config: DeepgramTTSConfig):
        self.config = config
        self.websocket_url = self._build_websocket_url()
        self.rest_url = self._build_rest_url()
        self.headers = {
            "Authorization": f"Token {config.api_key}"
        }
        
        # Persistent WebSocket connection management
        self.websocket: Optional[websockets.WebSocketServerProtocol] = None
        self.connection_lock = asyncio.Lock()
        self.is_connecting = False
        self.is_connected = False
        self.connection_task: Optional[asyncio.Task] = None
        self.message_handler_task: Optional[asyncio.Task] = None
        
        # Request management
        self.pending_requests: Dict[str, TTSRequest] = {}
        self.request_queue: Queue = Queue()
        self.ten_env: Optional[AsyncTenEnv] = None
        
        # Connection health
        self.last_ping_time = 0
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
        for request in self.pending_requests.values():
            if not request.completed:
                request.error = Exception("Connection closed")
                request.completed = True
                await request.audio_queue.put(None)  # Signal completion
        
        self.pending_requests.clear()
        
        if self.ten_env:
            self.ten_env.log_info("Deepgram TTS cleanup completed")

    async def _ensure_connection(self) -> bool:
        """Ensure WebSocket connection is established with health check"""
        async with self.connection_lock:
            # Check if connection is truly healthy
            if self.is_connected and self.websocket:
                # Perform health check
                if await self._health_check():
                    return True
                else:
                    if self.ten_env:
                        self.ten_env.log_warn("Connection health check failed, reconnecting...")
                    self.is_connected = False
            
            if self.is_connecting:
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
        for request in self.pending_requests.values():
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
        
        # For now, route to the most recent request
        # In a more sophisticated implementation, you'd need request correlation
        if self.pending_requests:
            # Get the most recent request
            latest_request = list(self.pending_requests.values())[-1]
            await latest_request.audio_queue.put(audio_data)

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
                # Signal completion for the most recent request
                if self.pending_requests:
                    latest_request = list(self.pending_requests.values())[-1]
                    latest_request.completed = True
                    await latest_request.audio_queue.put(None)  # Signal completion
                if self.ten_env:
                    self.ten_env.log_info("Audio stream flushed - request complete")
            elif message_type == "Error":
                error_msg = data.get("error", "Unknown error")
                if self.ten_env:
                    self.ten_env.log_error(f"Deepgram TTS error: {error_msg}")
                # Signal error for all pending requests
                for request in self.pending_requests.values():
                    if not request.completed:
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

    async def get(self, ten_env: AsyncTenEnv, text: str) -> AsyncIterator[bytes]:
        """
        Get TTS audio using persistent WebSocket connection with robust error handling
        """
        request_id = str(uuid.uuid4())
        request = TTSRequest(request_id, text)
        max_retries = 2
        
        for retry_attempt in range(max_retries + 1):
            try:
                # Ensure connection is available
                if not await self._ensure_connection():
                    ten_env.log_error("Failed to establish WebSocket connection")
                    if retry_attempt == max_retries:
                        if self.config.use_rest_fallback:
                            ten_env.log_info("All WebSocket attempts failed, falling back to REST API")
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        else:
                            raise Exception("WebSocket connection failed and REST API fallback disabled")
                    else:
                        ten_env.log_info(f"Retrying WebSocket connection (attempt {retry_attempt + 1}/{max_retries + 1})")
                        await asyncio.sleep(1.0 * (retry_attempt + 1))
                        continue
                
                # Add request to pending requests
                self.pending_requests[request_id] = request
                
                ten_env.log_info(f"Sending TTS request via persistent WebSocket: {text[:50]}... (attempt {retry_attempt + 1})")
                
                # Send the text to be synthesized
                message = {
                    "type": "Speak",
                    "text": text
                }
                
                await self.websocket.send(json.dumps(message))
                ten_env.log_info(f"Sent text to Deepgram via persistent connection")
                
                # Send Flush command to trigger audio generation
                flush_command = {"type": "Flush"}
                await self.websocket.send(json.dumps(flush_command))
                ten_env.log_info("Sent Flush command via persistent connection")
                
                # Stream audio chunks as they arrive
                chunk_count = 0
                request_timeout = self.config.websocket_timeout * 2  # Longer timeout for full request
                
                while not request.completed:
                    try:
                        # Wait for audio chunk with timeout
                        audio_chunk = await asyncio.wait_for(
                            request.audio_queue.get(), 
                            timeout=request_timeout if chunk_count == 0 else self.config.websocket_timeout
                        )
                        
                        if audio_chunk is None:  # Completion signal
                            break
                        
                        chunk_count += 1
                        ten_env.log_debug(f"Yielding audio chunk #{chunk_count} of size: {len(audio_chunk)} bytes")
                        yield audio_chunk
                        
                    except asyncio.TimeoutError:
                        ten_env.log_warn(f"Timeout waiting for audio chunk (request_id: {request_id}, chunk: {chunk_count})")
                        if chunk_count == 0:
                            # No audio received at all, this might be a connection issue
                            if not self.is_connected or not self.websocket:
                                ten_env.log_warn("Connection lost during request, will retry")
                                break
                        # If we got some chunks, continue waiting a bit more
                        if chunk_count > 0:
                            break
                        else:
                            # No chunks received, break and retry
                            break
                
                if request.error:
                    raise request.error
                
                if chunk_count > 0:
                    ten_env.log_info(f"TTS request completed successfully - yielded {chunk_count} chunks")
                    return  # Success, exit retry loop
                else:
                    ten_env.log_warn(f"No audio chunks received for request {request_id}")
                    if retry_attempt < max_retries:
                        ten_env.log_info("Retrying request...")
                        continue
                    else:
                        raise Exception("No audio data received after all retries")
                
            except (websockets.exceptions.ConnectionClosed,
                    websockets.exceptions.WebSocketException,
                    ConnectionResetError,
                    OSError) as e:
                ten_env.log_error(f"WebSocket connection error (attempt {retry_attempt + 1}): {str(e)}")
                self.is_connected = False
                
                if retry_attempt < max_retries:
                    ten_env.log_info("Connection error, retrying with new connection...")
                    await asyncio.sleep(1.0 * (retry_attempt + 1))
                    continue
                else:
                    # All retries failed, fall back to REST API
                    if self.config.use_rest_fallback:
                        ten_env.log_info("All WebSocket retries failed, falling back to REST API...")
                        try:
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        except Exception as rest_error:
                            ten_env.log_error(f"REST API fallback error: {str(rest_error)}")
                            raise
                    else:
                        raise
                        
            except Exception as e:
                ten_env.log_error(f"Unexpected error in TTS request (attempt {retry_attempt + 1}): {str(e)}")
                if retry_attempt < max_retries:
                    await asyncio.sleep(1.0 * (retry_attempt + 1))
                    continue
                else:
                    # Final attempt failed
                    if self.config.use_rest_fallback:
                        ten_env.log_info("Falling back to REST API after unexpected error...")
                        try:
                            async for chunk in self._get_rest_audio_streaming(ten_env, text):
                                yield chunk
                            return
                        except Exception as rest_error:
                            ten_env.log_error(f"REST API fallback error: {str(rest_error)}")
                            raise
                    else:
                        raise
            finally:
                # Clean up request
                if request_id in self.pending_requests:
                    del self.pending_requests[request_id]

    async def _get_rest_audio_streaming(self, ten_env: AsyncTenEnv, text: str) -> AsyncIterator[bytes]:
        """
        Get audio using REST API and stream it in chunks (fallback method)
        """
        try:
            ten_env.log_info(f"Using Deepgram REST API: {self.rest_url}")
            
            headers = {
                "Authorization": f"Token {self.config.api_key}",
                "Content-Type": "application/json"
            }
            
            data = {"text": text}
            
            async with aiohttp.ClientSession() as session:
                async with session.post(self.rest_url, headers=headers, json=data) as response:
                    if response.status == 200:
                        audio_data = await response.read()
                        ten_env.log_info(f"REST API generated {len(audio_data)} bytes")
                        
                        # Stream the audio in chunks
                        chunk_size = 4096
                        for i in range(0, len(audio_data), chunk_size):
                            chunk = audio_data[i:i + chunk_size]
                            yield chunk
                    else:
                        error_text = await response.text()
                        ten_env.log_error(f"REST API error {response.status}: {error_text}")
                        raise Exception(f"REST API error: {response.status} - {error_text}")
                        
        except Exception as e:
            ten_env.log_error(f"Error in Deepgram REST API: {str(e)}")
            raise
