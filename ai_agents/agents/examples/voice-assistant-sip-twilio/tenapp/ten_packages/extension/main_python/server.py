#!/usr/bin/env python3
"""
Main Python Server for Twilio Integration
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
from twilio.rest import Client
from twilio.twiml.voice_response import VoiceResponse

from .config import MainControlConfig


class TwilioCallServer:
    """Server for handling Twilio calls, media streaming, and webhooks"""

    def __init__(self, config: MainControlConfig, ten_env=None):
        self.config = config
        self.ten_env = ten_env
        self.app = FastAPI(title="Twilio Call Server")

        # Add CORS middleware
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],  # Allow all origins
            allow_credentials=True,
            allow_methods=["*"],  # Allow all methods
            allow_headers=["*"],  # Allow all headers
        )

        # Twilio client
        self.twilio_client = Client(
            config.twilio_account_sid, config.twilio_auth_token
        )

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
                message = body.get("message", "Hello from Twilio!")

                if not phone_number:
                    raise HTTPException(
                        status_code=400, detail="phone_number is required"
                    )

                self._log_info(
                    f"Creating call to {phone_number} with message: {message}"
                )

                # Create TwiML response
                twiml_response = VoiceResponse()

                # Add media stream if configured
                if self.config.twilio_public_server_url:
                    # For WebSocket, use configurable protocol (WSS or WS)
                    ws_protocol = "wss" if self.config.twilio_use_wss else "ws"
                    media_ws_url = f"{ws_protocol}://{self.config.twilio_public_server_url}/media"
                    self._log_info(
                        f"Adding media stream to WebSocket: {media_ws_url}"
                    )
                    connect = twiml_response.connect()
                    connect.stream(url=media_ws_url)
                    twiml_response.append(connect)
                    twiml_response.say("Stream Started")

                # twiml_response.say(message, voice="alice")

                # Configure webhook URL for call events
                if self.config.twilio_public_server_url:
                    http_protocol = (
                        "https" if self.config.twilio_use_https else "http"
                    )
                    webhook_url = f"{http_protocol}://{self.config.twilio_public_server_url}/webhook/status"
                else:
                    webhook_url = None

                if webhook_url:
                    self._log_info(f"Using webhook URL: {webhook_url}")
                else:
                    self._log_info(
                        "No public server URL configured - status callbacks will not be sent"
                    )

                # Create call parameters
                call_params = {
                    "to": phone_number,
                    "from_": self.config.twilio_from_number,
                    "twiml": str(twiml_response),
                }

                # Only add status callback if webhook URL is configured
                if webhook_url:
                    call_params["status_callback"] = webhook_url
                    call_params["status_callback_event"] = [
                        "initiated",
                        "ringing",
                        "answered",
                        "completed",
                    ]

                # Create the call
                call = self.twilio_client.calls.create(**call_params)

                # Store call session
                self.active_call_sessions[call.sid] = {
                    "phone_number": phone_number,
                    "message": message,
                    "call_sid": call.sid,
                    "status": "initiated",
                    "created_at": datetime.now().isoformat(),
                }

                self._log_info(f"Call created successfully: {call.sid}")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_sid": call.sid,
                        "status": "initiated",
                        "phone_number": phone_number,
                        "message": message,
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to create call: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.delete("/api/call/{call_sid}")
        async def end_call(call_sid: str):
            """End a call by SID"""
            try:
                if call_sid not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                self._log_info(f"Ending call: {call_sid}")

                # Update call status to completed
                call = self.twilio_client.calls(call_sid).update(
                    status="completed"
                )

                # Update session status
                if call_sid in self.active_call_sessions:
                    self.active_call_sessions[call_sid]["status"] = "completed"
                    self.active_call_sessions[call_sid][
                        "ended_at"
                    ] = datetime.now().isoformat()

                self._log_info(f"Call {call_sid} ended successfully")

                return JSONResponse(
                    content={
                        "success": True,
                        "call_sid": call_sid,
                        "status": "completed",
                    }
                )

            except Exception as e:
                self._log_error(f"Failed to end call {call_sid}: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/call/{call_sid}")
        async def get_call_status(call_sid: str):
            """Get call status by SID"""
            try:
                if call_sid not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                session = self.active_call_sessions[call_sid]

                return JSONResponse(
                    content={
                        "success": True,
                        "call_sid": call_sid,
                        "status": session["status"],
                        "phone_number": session["phone_number"],
                        "message": session["message"],
                        "created_at": session["created_at"],
                        "ended_at": session.get("ended_at"),
                    }
                )

            except Exception as e:
                self._log_error(
                    f"Failed to get call status {call_sid}: {str(e)}"
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
            """Handle Twilio status webhook"""
            try:
                # Handle both GET and POST requests
                if request.method == "GET":
                    # For GET requests, get parameters from query string
                    call_sid = request.query_params.get("CallSid")
                    call_status = request.query_params.get("CallStatus")
                    call_duration = request.query_params.get("CallDuration")
                    direction = request.query_params.get("Direction")
                else:
                    # For POST requests, get parameters from form data
                    form_data = await request.form()
                    call_sid = form_data.get("CallSid")
                    call_status = form_data.get("CallStatus")
                    call_duration = form_data.get("CallDuration")
                    direction = form_data.get("Direction")

                self._log_info(
                    f"Status webhook received for call {call_sid}: {call_status}"
                )

                # Update call session status
                if call_sid in self.active_call_sessions:
                    self.active_call_sessions[call_sid]["status"] = call_status

                    if call_status == "completed":
                        self.active_call_sessions[call_sid][
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

            if self.config.twilio_public_server_url:
                ws_protocol = "wss" if self.config.twilio_use_wss else "ws"
                http_protocol = (
                    "https" if self.config.twilio_use_https else "http"
                )
                media_ws_url = f"{ws_protocol}://{self.config.twilio_public_server_url}/media"
                webhook_url = f"{http_protocol}://{self.config.twilio_public_server_url}/webhook/status"

            return JSONResponse(
                content={
                    "twilio_from_number": self.config.twilio_from_number,
                    "server_port": self.config.twilio_server_port,
                    "public_server_url": (
                        self.config.twilio_public_server_url
                        if self.config.twilio_public_server_url
                        else None
                    ),
                    "use_https": self.config.twilio_use_https,
                    "use_wss": self.config.twilio_use_wss,
                    "media_stream_enabled": bool(
                        self.config.twilio_public_server_url
                    ),
                    "media_ws_url": media_ws_url,
                    "webhook_enabled": bool(
                        self.config.twilio_public_server_url
                    ),
                    "webhook_url": webhook_url,
                }
            )

        # WebSocket endpoint for media streaming
        @self.app.websocket("/media")
        async def websocket_endpoint(websocket: WebSocket):
            """WebSocket endpoint for Twilio media streaming"""
            self._log_info(
                f"WebSocket connection attempt from: {websocket.client}"
            )

            try:
                # Log connection attempt
                self._log_info(
                    f"WebSocket connection attempt from: {websocket.client}"
                )

                # Check for required query parameters (Twilio sends these)
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

                # Initialize call_sid to None to prevent NameError
                call_sid = None

                while True:
                    # Receive message from Twilio
                    data = await websocket.receive_text()
                    self._log_debug(
                        f"Received WebSocket message: {data[:100]}..."
                    )

                    # Parse Twilio media stream message
                    try:
                        import json

                        message = json.loads(data)

                        if message.get("event") == "media":
                            # Extract audio payload and call SID
                            audio_payload = message.get("media", {}).get(
                                "payload", ""
                            )
                            stream_sid = message.get("streamSid", "")

                            if audio_payload and call_sid:
                                # Forward audio to TEN framework
                                if (
                                    hasattr(self, "extension_instance")
                                    and self.extension_instance
                                ):
                                    await self.extension_instance._forward_audio_to_ten(
                                        audio_payload, stream_sid
                                    )
                                else:
                                    self._log_debug(
                                        "Extension instance not available for audio forwarding"
                                    )

                        elif message.get("event") == "start":
                            self._log_info(f"Media stream started: {message}")
                            stream_sid = message.get("streamSid", "")
                            start = message.get("start", {})
                            call_sid = start.get("callSid", "")
                            self.active_call_sessions[call_sid][
                                "stream_sid"
                            ] = stream_sid
                            self.active_call_sessions[call_sid][
                                "websocket"
                            ] = websocket

                            # Notify extension that websocket is connected
                            if (
                                hasattr(self, "extension_instance")
                                and self.extension_instance
                            ):
                                await self.extension_instance.on_websocket_connected(
                                    call_sid
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
        self._log_info(f"Starting Twilio Call Server on {host}:{port}")
        self._log_info(
            "Server supports both HTTP API and WebSocket media streaming on the same port"
        )

        # Check if SSL is required
        use_ssl = self.config.twilio_use_https or self.config.twilio_use_wss

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
        self._log_info("Cleaning up Twilio Call Server")
        # End all active calls
        for call_sid in list(self.active_call_sessions.keys()):
            try:
                self.twilio_client.calls(call_sid).update(status="completed")
                self._log_info(f"Ended call {call_sid}")
            except Exception as e:
                self._log_error(f"Failed to end call {call_sid}: {str(e)}")


async def main():
    """Main function to run the server"""
    # Load configuration from environment variables
    config = MainControlConfig(
        twilio_account_sid=os.getenv("TWILIO_ACCOUNT_SID", ""),
        twilio_auth_token=os.getenv("TWILIO_AUTH_TOKEN", ""),
        twilio_from_number=os.getenv("TWILIO_FROM_NUMBER", ""),
        twilio_server_port=int(os.getenv("TWILIO_SERVER_PORT", "8000")),
        twilio_public_server_url=os.getenv("TWILIO_PUBLIC_SERVER_URL", ""),
        twilio_use_https=os.getenv("TWILIO_USE_HTTPS", "true").lower()
        == "true",
        twilio_use_wss=os.getenv("TWILIO_USE_WSS", "true").lower() == "true",
    )

    # Validate required configuration
    if (
        not config.twilio_account_sid
        or not config.twilio_auth_token
        or not config.twilio_from_number
    ):
        print("Error: Missing required Twilio configuration")
        print(
            "Please set TWILIO_ACCOUNT_SID, TWILIO_AUTH_TOKEN, and TWILIO_FROM_NUMBER"
        )
        sys.exit(1)

    # Create and start server
    server = TwilioCallServer(config)

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
