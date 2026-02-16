#!/usr/bin/env python3
"""
Telnyx Server for Voice Call Handling
Handles call creation, media streaming, and webhook status
"""
import asyncio
import json
import os
import signal
import sys
from typing import Dict, Any, Optional
from datetime import datetime

import uvicorn
from fastapi import FastAPI, Request, HTTPException, WebSocket
from fastapi.responses import JSONResponse
from fastapi.middleware.cors import CORSMiddleware
from telnyx import Telnyx
from telnyx.api.call import Call
from telnyx.models.control import Control

from .config import TelnyxConfig


class TelnyxCallServer:
    """Server for handling Telnyx calls, media streaming, and webhooks"""

    def __init__(self, config: TelnyxConfig, ten_env=None):
        self.config = config
        self.ten_env = ten_env
        self.app = FastAPI(title="Telnyx Call Server")

        # Add CORS middleware
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],  # Allow all origins
            allow_credentials=True,
            allow_methods=["*"],  # Allow all methods
            allow_headers=["*"],  # Allow all headers
        )

        # Telnyx client
        self.telnyx_client = Telnyx(config.telnyx_api_key)

        # Active call sessions
        self.active_call_sessions: Dict[str, Dict[str, Any]] = {}

        # Setup routes
        self._setup_routes()

    def _log_info(self, message: str):
        """Log info message using ten_env if available"""
        if self.ten_env:
            self.ten_env.log_info(message)
        else:
            print(f"INFO: {message}")

    def _log_error(self, message: str):
        """Log error message using ten_env if available"""
        if self.ten_env:
            self.ten_env.log_error(message)
        else:
            print(f"ERROR: {message}")

    def _log_debug(self, message: str):
        """Log debug message using ten_env if available"""
        if self.ten_env:
            self.ten_env.log_debug(message)
        else:
            print(f"DEBUG: {message}")

    def _setup_routes(self):
        """Setup FastAPI routes"""

        @self.app.post("/api/call")
        async def create_call(request: Request):
            """Create a new outbound call"""
            try:
                body = await request.json()
                phone_number = body.get("phone_number")
                message = body.get("message", "Hello from Telnyx!")

                if not phone_number:
                    raise HTTPException(
                        status_code=400, detail="phone_number is required"
                    )

                self._log_info(
                    f"Creating call to {phone_number} with message: {message}"
                )

                # Build WebSocket URL for media streaming
                if self.config.telnyx_public_server_url:
                    ws_protocol = "wss" if self.config.telnyx_use_wss else "ws"
                    media_ws_url = f"{ws_protocol}://{self.config.telnyx_public_server_url}/media"
                    self._log_info(
                        f"Adding media stream to WebSocket: {media_ws_url}"
                    )
                else:
                    media_ws_url = None
                    self._log_info(
                        "No public server URL configured - media streaming disabled"
                    )

                # Create Telnyx call
                call_params = {
                    "to": phone_number,
                    "from_": self.config.telnyx_from_number,
                    "connection_id": self.config.telnyx_connection_id,
                    "answer_url": f"{media_ws_url}/control" if media_ws_url else None,
                }

                # Create the call using Telnyx SDK
                call = self.telnyx_client.calls.create(**call_params)

                # Store call session
                call_id = call.id if hasattr(call, 'id') else str(call)
                self.active_call_sessions[call_id] = {
                    "phone_number": phone_number,
                    "message": message,
                    "call_id": call_id,
                    "status": "initiated",
                    "created_at": datetime.now().isoformat(),
                }

                self._log_info(f"Call created successfully: {call_id}")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_id": call_id,
                        "status": "initiated",
                        "phone_number": phone_number,
                        "message": message,
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to create call: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.delete("/api/call/{call_id}")
        async def end_call(call_id: str):
            """End a call by ID"""
            try:
                if call_id not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                self._log_info(f"Ending call: {call_id}")

                # End call via Telnyx API
                self.telnyx_client.calls(call_id).update(
                    status="completed"
                )

                # Update session status
                if call_id in self.active_call_sessions:
                    self.active_call_sessions[call_id]["status"] = "completed"
                    self.active_call_sessions[call_id][
                        "ended_at"
                    ] = datetime.now().isoformat()

                self._log_info(f"Call {call_id} ended successfully")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_id": call_id,
                        "status": "completed",
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to end call {call_id}: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/call/{call_id}")
        async def get_call_status(call_id: str):
            """Get call status by ID"""
            try:
                if call_id not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                session = self.active_call_sessions[call_id]

                return JSONResponse(
                    content={
                        "success": True,
                        "call_id": call_id,
                        "status": session["status"],
                        "phone_number": session["phone_number"],
                        "message": session["message"],
                        "created_at": session["created_at"],
                        "ended_at": session.get("ended_at"),
                    }
                )

            except Exception as e:
                self._log_error(
                    f"Failed to get call status {call_id}: {str(e)}"
                )
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/calls")
        async def list_calls():
            """List all active calls"""
            return JSONResponse(
                content={
                    "success": True,
                    "active_calls": len(self.active_call_sessions),
                    "calls": list(self.active_call_sessions.keys()),
                }
            )

        @self.app.post("/webhook/status")
        @self.app.get("/webhook/status")
        async def handle_status_webhook(request: Request):
            """Handle Telnyx status webhook"""
            try:
                # Handle both GET and POST requests
                if request.method == "GET":
                    # For GET requests, get parameters from query string
                    call_id = request.query_params.get("CallSid") or request.query_params.get("call_id")
                    call_status = request.query_params.get("CallStatus") or request.query_params.get("status")
                    call_duration = request.query_params.get("CallDuration") or request.query_params.get("duration")
                else:
                    # For POST requests, get parameters from form data
                    form_data = await request.form()
                    call_id = form_data.get("CallSid") or form_data.get("call_id")
                    call_status = form_data.get("CallStatus") or form_data.get("status")
                    call_duration = form_data.get("CallDuration") or form_data.get("duration")

                self._log_info(
                    f"Status webhook received for call {call_id}: {call_status}"
                )

                # Update call session status
                if call_id in self.active_call_sessions:
                    self.active_call_sessions[call_id]["status"] = call_status

                    if call_status == "completed":
                        self.active_call_sessions[call_id][
                            "ended_at"
                        ] = datetime.now().isoformat()

                return JSONResponse(content={"success": True})

            except Exception as e:
                self._log_error(f"Failed to handle status webhook: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/health")
        async def health_check():
            """Health check endpoint"""
            return JSONResponse(
                content={
                    "status": "healthy",
                    "active_calls": len(self.active_call_sessions),
                    "server_time": datetime.now().isoformat(),
                }
            )

        @self.app.get("/api/config")
        async def get_config():
            """Get server configuration"""
            # Build URLs with configurable protocols
            media_ws_url = None
            webhook_url = None

            if self.config.telnyx_public_server_url:
                ws_protocol = "wss" if self.config.telnyx_use_wss else "ws"
                http_protocol = "https" if self.config.telnyx_use_https else "http"
                media_ws_url = f"{ws_protocol}://{self.config.telnyx_public_server_url}/media"
                webhook_url = f"{http_protocol}://{self.config.telnyx_public_server_url}/webhook/status"

            return JSONResponse(
                content={
                    "telnyx_from_number": self.config.telnyx_from_number,
                    "server_port": self.config.telnyx_server_port,
                    "public_server_url": (
                        self.config.telnyx_public_server_url
                        if self.config.telnyx_public_server_url
                        else None
                    ),
                    "use_https": self.config.telnyx_use_https,
                    "use_wss": self.config.telnyx_use_wss,
                    "media_stream_enabled": bool(
                        self.config.telnyx_public_server_url
                    ),
                    "media_ws_url": media_ws_url,
                    "webhook_enabled": bool(
                        self.config.telnyx_public_server_url
                    ),
                    "webhook_url": webhook_url,
                }
            )

        # WebSocket endpoint for media streaming
        @self.app.websocket("/media")
        async def websocket_endpoint(websocket: WebSocket):
            """WebSocket endpoint for Telnyx media streaming"""
            self._log_info(
                f"WebSocket connection attempt from: {websocket.client}"
            )

            try:
                # Accept the connection immediately
                await websocket.accept()
                self._log_info(
                    f"WebSocket connection established: {websocket.client}"
                )

                # Send initial message to confirm connection
                await websocket.send_text(
                    '{"type": "connected", "message": "WebSocket connection established"}'
                )

                # Initialize call_id to None to prevent NameError
                call_id = None

                while True:
                    # Receive message from Telnyx
                    data = await websocket.receive_text()
                    self._log_debug(
                        f"Received WebSocket message: {data[:100]}..."
                    )

                    # Parse Telnyx media stream message
                    try:
                        import json

                        message = json.loads(data)

                        if message.get("event") == "media":
                            # Extract audio payload and call ID
                            audio_payload = message.get("media", {}).get(
                                "payload", ""
                            )
                            stream_id = message.get("streamSid", "")

                            if audio_payload and call_id:
                                # Forward audio to TEN framework
                                if (
                                    hasattr(self, "extension_instance")
                                    and self.extension_instance
                                ):
                                    await self.extension_instance._forward_audio_to_ten(
                                        audio_payload, stream_id
                                    )
                                else:
                                    self._log_debug(
                                        "Extension instance not available for audio forwarding"
                                    )

                        elif message.get("event") == "start":
                            self._log_info(f"Media stream started: {message}")
                            stream_id = message.get("streamSid", "")
                            start = message.get("start", {})
                            call_id = start.get("callSid", "") or start.get("call_id", "")
                            self.active_call_sessions[call_id][
                                "stream_id"
                            ] = stream_id
                            self.active_call_sessions[call_id][
                                "websocket"
                            ] = websocket

                            # Notify extension that websocket is connected
                            if (
                                hasattr(self, "extension_instance")
                                and self.extension_instance
                            ):
                                await self.extension_instance.on_websocket_connected(
                                    call_id
                                )
                        elif message.get("event") == "stop":
                            self._log_info(f"Media stream stopped: {message}")

                    except json.JSONDecodeError:
                        self._log_debug(
                            f"Received non-JSON message: {data[:100]}..."
                        )
                    except Exception as e:
                        self._log_error(f"Error processing media message: {e}")

            except Exception as e:
                self._log_error(f"WebSocket error: {e}")
                # Try to close the connection gracefully
                try:
                    await websocket.close()
                except:
                    pass
            finally:
                self._log_info("WebSocket connection closed")

    async def start_server(self, host: str = "0.0.0.0", port: int = 8080):
        """Start the server with both HTTP and WebSocket support"""
        self._log_info(f"Starting Telnyx Call Server on {host}:{port}")
        self._log_info(
            "Server supports both HTTP API and WebSocket media streaming on the same port"
        )

        # Check if SSL is required
        use_ssl = self.config.telnyx_use_https or self.config.telnyx_use_wss

        if use_ssl:
            # For development with ngrok, we'll use HTTP but let ngrok handle SSL
            self._log_info(
                "SSL/WSS requested - using HTTP server (ngrok will handle SSL termination)"
            )
            ssl_keyfile = None
            ssl_certfile = None
        else:
            ssl_keyfile = None
            ssl_certfile = None

        # Start server with HTTP and WebSocket support
        config = uvicorn.Config(
            app=self.app,
            host=host,
            port=port,
            log_level="info",
            ssl_keyfile=ssl_keyfile,
            ssl_certfile=ssl_certfile,
        )

        server = uvicorn.Server(config)
        await server.serve()

    def cleanup(self):
        """Cleanup resources"""
        self._log_info("Cleaning up Telnyx Call Server")
        # End all active calls
        for call_id in list(self.active_call_sessions.keys()):
            try:
                self.telnyx_client.calls(call_id).update(status="completed")
                self._log_info(f"Ended call {call_id}")
            except Exception as e:
                self._log_error(f"Failed to end call {call_id}: {str(e)}")


async def main():
    """Main function to run the server"""
    # Load configuration from environment variables
    config = TelnyxConfig(
        telnyx_api_key=os.getenv("TELNYX_API_KEY", ""),
        telnyx_connection_id=os.getenv("TELNYX_CONNECTION_ID", ""),
        telnyx_from_number=os.getenv("TELNYX_FROM_NUMBER", ""),
        telnyx_server_port=int(os.getenv("TELNYX_SERVER_PORT", "8000")),
        telnyx_public_server_url=os.getenv("TELNYX_PUBLIC_SERVER_URL", ""),
        telnyx_use_https=os.getenv("TELNYX_USE_HTTPS", "true").lower()
        == "true",
        telnyx_use_wss=os.getenv("TELNYX_USE_WSS", "true").lower() == "true",
    )

    # Validate required configuration
    if (
        not config.telnyx_api_key
        or not config.telnyx_connection_id
        or not config.telnyx_from_number
    ):
        print("Error: Missing required Telnyx configuration")
        print(
            "Please set TELNYX_API_KEY, TELNYX_CONNECTION_ID, and TELNYX_FROM_NUMBER"
        )
        sys.exit(1)

    # Create and start server
    server = TelnyxCallServer(config)

    # Setup signal handlers for graceful shutdown
    def signal_handler(signum, frame):
        print(f"Received signal {signum}, shutting down...")
        server.cleanup()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    try:
        await server.start_server()
    except KeyboardInterrupt:
        print("Server interrupted, shutting down...")
        server.cleanup()
    except Exception as e:
        print(f"Server error: {e}")
        server.cleanup()
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())