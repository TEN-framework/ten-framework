"""
WebSocket Server Manager for receiving audio data
"""

import asyncio
import json
import base64
from typing import Callable, Optional, Any
from dataclasses import dataclass
import websockets
from websockets.server import WebSocketServerProtocol
from ten.async_ten_env import AsyncTenEnv


@dataclass
class AudioData:
    """Container for audio data with metadata"""

    pcm_data: bytes
    client_id: str
    metadata: dict[str, Any]


class WebSocketServerManager:
    """Manages WebSocket server and client connections"""

    def __init__(
        self,
        host: str,
        port: int,
        ten_env: AsyncTenEnv,
        on_audio_callback: Optional[Callable[[AudioData], None]] = None,
    ):
        """
        Initialize WebSocket server manager

        Args:
            host: Server host address
            port: Server port
            ten_env: TEN environment for logging
            on_audio_callback: Callback when audio data is received
        """
        self.host = host
        self.port = port
        self.ten_env = ten_env
        self.on_audio_callback = on_audio_callback

        self.server = None
        self.clients: set[WebSocketServerProtocol] = set()
        self.running = False
        self._server_task: Optional[asyncio.Task] = None

    async def start(self) -> None:
        """Start the WebSocket server"""
        if self.running:
            self.ten_env.log_warn("WebSocket server already running")
            return

        self.running = True
        try:
            self.server = await websockets.serve(
                self._handle_client, self.host, self.port
            )
            self.ten_env.log_info(
                f"WebSocket server started on ws://{self.host}:{self.port}"
            )
        except Exception as e:
            self.ten_env.log_error(f"Failed to start WebSocket server: {e}")
            self.running = False
            raise

    async def stop(self) -> None:
        """Stop the WebSocket server and close all connections"""
        if not self.running:
            return

        self.running = False
        self.ten_env.log_info("Stopping WebSocket server...")

        # Close all client connections
        if self.clients:
            await asyncio.gather(
                *[self._close_client(client) for client in list(self.clients)],
                return_exceptions=True,
            )

        # Close server
        if self.server:
            self.server.close()
            await self.server.wait_closed()

        self.ten_env.log_info("WebSocket server stopped")

    async def _handle_client(self, websocket: WebSocketServerProtocol) -> None:
        """
        Handle a single WebSocket client connection

        Args:
            websocket: WebSocket connection
        """
        client_id = f"{websocket.remote_address[0]}:{websocket.remote_address[1]}"
        self.clients.add(websocket)
        self.ten_env.log_info(
            f"Client connected: {client_id} (total clients: {len(self.clients)})"
        )

        try:
            async for message in websocket:
                if not self.running:
                    break

                await self._process_message(message, websocket, client_id)

        except websockets.exceptions.ConnectionClosed:
            self.ten_env.log_info(f"Client disconnected: {client_id}")
        except Exception as e:
            self.ten_env.log_error(f"Error handling client {client_id}: {e}")
            await self._send_error(websocket, f"Server error: {str(e)}")
        finally:
            self.clients.discard(websocket)
            self.ten_env.log_info(
                f"Client removed: {client_id} (remaining clients: {len(self.clients)})"
            )

    async def _process_message(
        self, message: str, websocket: WebSocketServerProtocol, client_id: str
    ) -> None:
        """
        Process incoming message from client

        Args:
            message: Raw message string
            websocket: Client WebSocket connection
            client_id: Client identifier
        """
        try:
            # Parse JSON message
            data = json.loads(message)

            # Validate message format
            if "audio" not in data:
                await self._send_error(
                    websocket, 'Missing required field: "audio" with base64 data'
                )
                return

            # Decode base64 audio
            try:
                audio_base64 = data["audio"]
                pcm_data = base64.b64decode(audio_base64)
            except Exception as e:
                await self._send_error(websocket, f"Invalid base64 audio data: {e}")
                return

            # Extract metadata
            metadata = data.get("metadata", {})
            metadata["client_id"] = client_id

            # Create audio data container
            audio_data = AudioData(
                pcm_data=pcm_data, client_id=client_id, metadata=metadata
            )

            # Call callback to process audio
            if self.on_audio_callback:
                try:
                    await self.on_audio_callback(audio_data)
                except Exception as e:
                    self.ten_env.log_error(f"Error in audio callback: {e}")
                    await self._send_error(websocket, f"Processing error: {str(e)}")

        except json.JSONDecodeError as e:
            await self._send_error(websocket, f"Invalid JSON: {e}")
        except Exception as e:
            self.ten_env.log_error(f"Error processing message from {client_id}: {e}")
            await self._send_error(websocket, f"Processing error: {str(e)}")

    async def _send_error(self, websocket: WebSocketServerProtocol, error: str) -> None:
        """
        Send error message to client

        Args:
            websocket: Client WebSocket connection
            error: Error message
        """
        try:
            error_msg = json.dumps({"type": "error", "error": error})
            await websocket.send(error_msg)
        except Exception as e:
            self.ten_env.log_error(f"Failed to send error to client: {e}")

    async def broadcast(self, message: dict[str, Any]) -> None:
        """
        Broadcast message to all connected clients

        Args:
            message: Message dictionary to send
        """
        if not self.clients:
            return

        message_str = json.dumps(message)
        await asyncio.gather(
            *[self._send_to_client(client, message_str) for client in self.clients],
            return_exceptions=True,
        )

    async def send_audio_to_clients(
        self, pcm_data: bytes, metadata: Optional[dict[str, Any]] = None
    ) -> None:
        """
        Send audio data to all connected WebSocket clients

        Args:
            pcm_data: Raw PCM audio bytes
            metadata: Optional metadata to include with the audio
        """
        if not self.clients:
            self.ten_env.log_debug("No clients connected, skipping audio send")
            return

        try:
            # Encode PCM to base64
            audio_base64 = base64.b64encode(pcm_data).decode("utf-8")

            # Build message
            message = {"type": "audio", "audio": audio_base64}

            if metadata:
                message["metadata"] = metadata

            # Broadcast to all clients
            await self.broadcast(message)

            self.ten_env.log_debug(
                f"Sent {len(pcm_data)} bytes of audio to {len(self.clients)} client(s)"
            )

        except Exception as e:
            self.ten_env.log_error(f"Error sending audio to clients: {e}")

    async def send_to_client(
        self, client_id: str, message: dict[str, Any]
    ) -> bool:
        """
        Send message to a specific client

        Args:
            client_id: Client identifier
            message: Message dictionary to send

        Returns:
            True if sent successfully, False otherwise
        """
        message_str = json.dumps(message)

        for client in self.clients:
            if f"{client.remote_address[0]}:{client.remote_address[1]}" == client_id:
                return await self._send_to_client(client, message_str)

        self.ten_env.log_warn(f"Client {client_id} not found")
        return False

    async def _send_to_client(
        self, websocket: WebSocketServerProtocol, message: str
    ) -> bool:
        """
        Send message to a WebSocket client

        Args:
            websocket: Client WebSocket connection
            message: Message string

        Returns:
            True if sent successfully, False otherwise
        """
        try:
            await websocket.send(message)
            return True
        except Exception as e:
            self.ten_env.log_error(f"Failed to send message to client: {e}")
            return False

    async def _close_client(self, websocket: WebSocketServerProtocol) -> None:
        """
        Close a client connection gracefully

        Args:
            websocket: Client WebSocket connection
        """
        try:
            await websocket.close()
        except Exception as e:
            self.ten_env.log_error(f"Error closing client connection: {e}")

    def get_client_count(self) -> int:
        """Get number of connected clients"""
        return len(self.clients)
