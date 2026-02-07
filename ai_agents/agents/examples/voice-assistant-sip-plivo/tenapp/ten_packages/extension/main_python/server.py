#!/usr/bin/env python3
"""
Main Python Server for Plivo Integration
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
from fastapi.responses import JSONResponse, Response
from fastapi.middleware.cors import CORSMiddleware
import plivo
from plivo import plivoxml

from .config import MainControlConfig


class PlivoCallServer:
    """Server for handling Plivo calls, media streaming, and webhooks"""

    def __init__(self, config: MainControlConfig, ten_env=None):
        self.config = config
        self.ten_env = ten_env
        self.app = FastAPI(title="Plivo Call Server")

        # Add CORS middleware
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],  # Allow all origins
            allow_credentials=True,
            allow_methods=["*"],  # Allow all methods
            allow_headers=["*"],  # Allow all headers
        )

        # Plivo client
        self.plivo_client = plivo.RestClient(
            config.plivo_auth_id, config.plivo_auth_token
        )

        # Active call sessions (keyed by call_uuid)
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
                message = body.get("message", "Hello from Plivo!")

                if not phone_number:
                    raise HTTPException(
                        status_code=400, detail="phone_number is required"
                    )

                self._log_info(
                    f"Creating call to {phone_number} with message: {message}"
                )

                # Configure webhook URL for answering the call
                if self.config.plivo_public_server_url:
                    http_protocol = (
                        "https" if self.config.plivo_use_https else "http"
                    )
                    answer_url = f"{http_protocol}://{self.config.plivo_public_server_url}/webhook/answer"
                    status_url = f"{http_protocol}://{self.config.plivo_public_server_url}/webhook/status"
                else:
                    raise HTTPException(
                        status_code=400,
                        detail="plivo_public_server_url is required for outbound calls"
                    )

                self._log_info(f"Using answer URL: {answer_url}")
                self._log_info(f"Using status URL: {status_url}")

                # Create the call using Plivo API
                response = self.plivo_client.calls.create(
                    from_=self.config.plivo_from_number,
                    to_=phone_number,
                    answer_url=answer_url,
                    answer_method="POST",
                    hangup_url=status_url,
                    hangup_method="POST",
                )

                call_uuid = response.request_uuid

                # Store call session
                self.active_call_sessions[call_uuid] = {
                    "phone_number": phone_number,
                    "message": message,
                    "call_uuid": call_uuid,
                    "status": "initiated",
                    "created_at": datetime.now().isoformat(),
                }

                self._log_info(f"Call created successfully: {call_uuid}")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_uuid": call_uuid,
                        "status": "initiated",
                        "phone_number": phone_number,
                        "message": message,
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to create call: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.delete("/api/call/{call_uuid}")
        async def end_call(call_uuid: str):
            """End a call by UUID"""
            try:
                if call_uuid not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                self._log_info(f"Ending call: {call_uuid}")

                # Hangup the call using Plivo API
                self.plivo_client.calls.delete(call_uuid)

                # Update session status
                if call_uuid in self.active_call_sessions:
                    self.active_call_sessions[call_uuid]["status"] = "completed"
                    self.active_call_sessions[call_uuid][
                        "ended_at"
                    ] = datetime.now().isoformat()

                self._log_info(f"Call {call_uuid} ended successfully")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_uuid": call_uuid,
                        "status": "completed",
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to end call {call_uuid}: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/call/{call_uuid}")
        async def get_call_status(call_uuid: str):
            """Get call status by UUID"""
            try:
                if call_uuid not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                session = self.active_call_sessions[call_uuid]

                return JSONResponse(
                    content={
                        "success": True,
                        "call_uuid": call_uuid,
                        "status": session["status"],
                        "phone_number": session["phone_number"],
                        "message": session["message"],
                        "created_at": session["created_at"],
                        "ended_at": session.get("ended_at"),
                    }
                )

            except Exception as e:
                self._log_error(
                    f"Failed to get call status {call_uuid}: {str(e)}"
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

        @self.app.post("/webhook/answer")
        @self.app.get("/webhook/answer")
        async def handle_answer_webhook(request: Request):
            """Handle Plivo answer webhook - returns XML to start media stream"""
            try:
                # Get call UUID from request
                if request.method == "GET":
                    call_uuid = request.query_params.get("CallUUID", "")
                else:
                    form_data = await request.form()
                    call_uuid = form_data.get("CallUUID", "")

                self._log_info(f"Answer webhook received for call {call_uuid}")

                # Build media stream WebSocket URL
                ws_protocol = "wss" if self.config.plivo_use_wss else "ws"
                media_ws_url = f"{ws_protocol}://{self.config.plivo_public_server_url}/media"

                self._log_info(f"Media stream URL: {media_ws_url}")

                # Create Plivo XML response with bidirectional stream
                response = plivoxml.ResponseElement()
                response.add(
                    plivoxml.StreamElement(
                        media_ws_url,
                        bidirectional="true",
                        keep_call_alive="true",
                        content_type="audio/x-mulaw;rate=8000"
                    )
                )

                xml_response = response.to_string()
                self._log_info(f"Plivo XML response: {xml_response}")

                return Response(
                    content=xml_response,
                    media_type="application/xml"
                )

            except Exception as e:
                self._log_error(f"Failed to handle answer webhook: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.post("/webhook/status")
        @self.app.get("/webhook/status")
        async def handle_status_webhook(request: Request):
            """Handle Plivo status webhook"""
            try:
                # Handle both GET and POST requests
                if request.method == "GET":
                    call_uuid = request.query_params.get("CallUUID")
                    call_status = request.query_params.get("CallStatus")
                    duration = request.query_params.get("Duration")
                    direction = request.query_params.get("Direction")
                else:
                    form_data = await request.form()
                    call_uuid = form_data.get("CallUUID")
                    call_status = form_data.get("CallStatus")
                    duration = form_data.get("Duration")
                    direction = form_data.get("Direction")

                self._log_info(
                    f"Status webhook received for call {call_uuid}: {call_status}"
                )

                # Update call session status
                if call_uuid in self.active_call_sessions:
                    self.active_call_sessions[call_uuid]["status"] = call_status

                    if call_status in ["completed", "hangup"]:
                        self.active_call_sessions[call_uuid][
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

            if self.config.plivo_public_server_url:
                ws_protocol = "wss" if self.config.plivo_use_wss else "ws"
                http_protocol = (
                    "https" if self.config.plivo_use_https else "http"
                )
                media_ws_url = f"{ws_protocol}://{self.config.plivo_public_server_url}/media"
                webhook_url = f"{http_protocol}://{self.config.plivo_public_server_url}/webhook/status"

            return JSONResponse(
                content={
                    "plivo_from_number": self.config.plivo_from_number,
                    "server_port": self.config.plivo_server_port,
                    "public_server_url": (
                        self.config.plivo_public_server_url
                        if self.config.plivo_public_server_url
                        else None
                    ),
                    "use_https": self.config.plivo_use_https,
                    "use_wss": self.config.plivo_use_wss,
                    "media_stream_enabled": bool(
                        self.config.plivo_public_server_url
                    ),
                    "media_ws_url": media_ws_url,
                    "webhook_enabled": bool(
                        self.config.plivo_public_server_url
                    ),
                    "webhook_url": webhook_url,
                }
            )

        # WebSocket endpoint for media streaming
        @self.app.websocket("/media")
        async def websocket_endpoint(websocket: WebSocket):
            """WebSocket endpoint for Plivo media streaming"""
            self._log_info(
                f"WebSocket connection attempt from: {websocket.client}"
            )

            try:
                # Log connection attempt
                self._log_info(
                    f"WebSocket connection attempt from: {websocket.client}"
                )

                # Check for required query parameters (Plivo sends these)
                query_params = websocket.query_params
                self._log_info(
                    f"WebSocket query parameters: {dict(query_params)}"
                )

                # Accept the connection immediately
                await websocket.accept()
                self._log_info(
                    f"WebSocket connection established: {websocket.client}"
                )

                # Send initial message to confirm connection
                await websocket.send_text(
                    '{"type": "connected", "message": "WebSocket connection established"}'
                )

                # Initialize call_uuid to None to prevent NameError
                call_uuid = None

                while True:
                    # Receive message from Plivo
                    data = await websocket.receive_text()
                    self._log_debug(
                        f"Received WebSocket message: {data[:100]}..."
                    )

                    # Parse Plivo media stream message
                    try:
                        message = json.loads(data)

                        if message.get("event") == "media":
                            # Extract audio payload
                            # Plivo format: {"event": "media", "media": {"payload": "base64...", "track": "inbound"}}
                            audio_payload = message.get("media", {}).get(
                                "payload", ""
                            )
                            stream_id = message.get("streamId", "")

                            if audio_payload and call_uuid:
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
                            # Plivo format: {"event": "start", "start": {"streamId": "...", "callId": "..."}}
                            stream_id = message.get("streamId", "")
                            start = message.get("start", {})
                            call_uuid = start.get("callId", "")

                            # Create session if it doesn't exist (for inbound calls)
                            if call_uuid not in self.active_call_sessions:
                                self.active_call_sessions[call_uuid] = {
                                    "call_uuid": call_uuid,
                                    "status": "in-progress",
                                    "created_at": datetime.now().isoformat(),
                                }

                            self.active_call_sessions[call_uuid][
                                "stream_id"
                            ] = stream_id
                            self.active_call_sessions[call_uuid][
                                "websocket"
                            ] = websocket

                            # Notify extension that websocket is connected
                            if (
                                hasattr(self, "extension_instance")
                                and self.extension_instance
                            ):
                                await self.extension_instance.on_websocket_connected(
                                    call_uuid
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

    async def start_server(self, host: str = "0.0.0.0", port: int = 9000):
        """Start the server with both HTTP and WebSocket support"""
        self._log_info(f"Starting Plivo Call Server on {host}:{port}")
        self._log_info(
            "Server supports both HTTP API and WebSocket media streaming on the same port"
        )

        # Check if SSL is required
        use_ssl = self.config.plivo_use_https or self.config.plivo_use_wss

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
        self._log_info("Cleaning up Plivo Call Server")
        # End all active calls
        for call_uuid in list(self.active_call_sessions.keys()):
            try:
                self.plivo_client.calls.delete(call_uuid)
                self._log_info(f"Ended call {call_uuid}")
            except Exception as e:
                self._log_error(f"Failed to end call {call_uuid}: {str(e)}")


async def main():
    """Main function to run the server"""
    # Load configuration from environment variables
    config = MainControlConfig(
        plivo_auth_id=os.getenv("PLIVO_AUTH_ID", ""),
        plivo_auth_token=os.getenv("PLIVO_AUTH_TOKEN", ""),
        plivo_from_number=os.getenv("PLIVO_FROM_NUMBER", ""),
        plivo_server_port=int(os.getenv("PLIVO_SERVER_PORT", "9000")),
        plivo_public_server_url=os.getenv("PLIVO_PUBLIC_SERVER_URL", ""),
        plivo_use_https=os.getenv("PLIVO_USE_HTTPS", "true").lower()
        == "true",
        plivo_use_wss=os.getenv("PLIVO_USE_WSS", "true").lower() == "true",
    )

    # Validate required configuration
    if (
        not config.plivo_auth_id
        or not config.plivo_auth_token
        or not config.plivo_from_number
    ):
        print("Error: Missing required Plivo configuration")
        print(
            "Please set PLIVO_AUTH_ID, PLIVO_AUTH_TOKEN, and PLIVO_FROM_NUMBER"
        )
        sys.exit(1)

    # Create and start server
    server = PlivoCallServer(config)

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
