#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import base64
import time
from typing import Dict, Any, Optional
from fastapi import FastAPI, HTTPException, Request, WebSocket
from fastapi.responses import JSONResponse, Response
import uvicorn

# Twilio imports
from twilio.rest import Client
from twilio.twiml.voice_response import VoiceResponse

from ten_runtime import AsyncTenEnv, Loc

from .config import MainControlConfig


class TwilioServer:
    """FastAPI server for Twilio integration"""

    def __init__(self, config: MainControlConfig, ten_env=None):
        self.config = config
        self.ten_env: AsyncTenEnv = ten_env
        self.app = FastAPI(title="Twilio Dial Server", version="1.0.0")
        self.active_call_sessions: Dict[str, Dict[str, Any]] = {}
        self.twilio_client: Optional[Client] = None
        self._setup_routes()
        self._setup_twilio_client()

    def _setup_twilio_client(self):
        """Initialize Twilio client"""
        if self.config.twilio_account_sid and self.config.twilio_auth_token:
            try:
                self.twilio_client = Client(
                    self.config.twilio_account_sid,
                    self.config.twilio_auth_token,
                )
                if self.ten_env:
                    self.ten_env.log_info(
                        "Twilio client initialized successfully"
                    )
            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(
                        f"Failed to initialize Twilio client: {e}"
                    )

    def _setup_routes(self):
        """Setup FastAPI routes"""

        @self.app.post("/api/calls")
        async def create_call(request: Request):
            """Create a new outbound call"""
            try:
                body = await request.json()
                phone_number = body.get("phone_number")
                message = body.get(
                    "message", "Hello, this is a call from the AI assistant."
                )

                if not phone_number:
                    raise HTTPException(
                        status_code=400, detail="phone_number is required"
                    )

                if not self.twilio_client:
                    raise HTTPException(
                        status_code=500, detail="Twilio client not initialized"
                    )

                # Create TwiML response with WebSocket streaming
                twiml_response = VoiceResponse()
                twiml_response.say(message, voice="alice")

                # Use twilio_client_media_ws_url for WebSocket URL
                if not self.config.twilio_client_media_ws_url:
                    raise HTTPException(
                        status_code=500,
                        detail="Twilio client media WebSocket URL is empty",
                    )

                # Use twilio_client_media_ws_url as-is for WebSocket URL
                websocket_url = (
                    f"wss://{self.config.twilio_client_media_ws_url}/ws"
                )

                twiml_response.start().stream(url=websocket_url)
                twiml_response.pause(length=30)

                # Add status callback for call events
                # For status callback, use the configured HTTP port
                if ":" in self.config.twilio_client_media_ws_url:
                    # Domain already includes port, use as-is
                    status_callback_url = f"https://{self.config.twilio_client_media_ws_url}/webhook/status"
                else:
                    # No port in webhook URL, add HTTP port
                    status_callback_url = f"https://{self.config.twilio_client_media_ws_url}:{self.config.twilio_server_webhook_http_port}/webhook/status"

                # Make the call with status callback
                call = self.twilio_client.calls.create(
                    to=phone_number,
                    from_=self.config.twilio_from_number,
                    twiml=str(twiml_response),
                    status_callback=status_callback_url,
                    status_callback_event=[
                        "initiated",
                        "ringing",
                        "answered",
                        "completed",
                    ],
                )

                # Store call session
                self.active_call_sessions[call.sid] = {
                    "phone_number": phone_number,
                    "message": message,
                    "status": call.status,
                    "call_type": "outbound",
                    "websocket": None,
                    "created_at": time.time(),
                }

                if self.ten_env:
                    self.ten_env.log_info(
                        f"Outbound call initiated: SID={call.sid}, To={phone_number}"
                    )

                return JSONResponse(
                    status_code=201,
                    content={
                        "call_sid": call.sid,
                        "phone_number": phone_number,
                        "message": message,
                        "status": call.status,
                        "created_at": time.time(),
                    },
                )

            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Failed to start call: {e}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/calls/{call_sid}")
        async def get_call(call_sid: str):
            """Get call information"""
            if call_sid not in self.active_call_sessions:
                raise HTTPException(status_code=404, detail="Call not found")

            session = self.active_call_sessions[call_sid]
            return JSONResponse(
                content={
                    "call_sid": call_sid,
                    "phone_number": session["phone_number"],
                    "status": session["status"],
                    "created_at": session["created_at"],
                    "has_websocket": session.get("websocket") is not None,
                }
            )

        @self.app.delete("/api/calls/{call_sid}")
        async def delete_call(call_sid: str):
            """Stop and delete a call"""
            try:
                if call_sid not in self.active_call_sessions:
                    raise HTTPException(
                        status_code=404, detail="Call not found"
                    )

                if self.twilio_client:
                    # Hang up the call via Twilio API
                    call = self.twilio_client.calls(call_sid).update(
                        status="completed"
                    )

                # Remove from active sessions
                del self.active_call_sessions[call_sid]

                if self.ten_env:
                    self.ten_env.log_info(f"Call {call_sid} stopped")

                return JSONResponse(
                    content={"message": f"Call {call_sid} stopped"}
                )

            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"Failed to stop call: {e}")
                raise HTTPException(status_code=500, detail=str(e))

        @self.app.get("/api/calls")
        async def list_calls():
            """List all active calls"""
            calls = []
            for call_sid, session in self.active_call_sessions.items():
                calls.append(
                    {
                        "call_sid": call_sid,
                        "phone_number": session["phone_number"],
                        "status": session["status"],
                        "created_at": session["created_at"],
                        "has_websocket": session.get("websocket") is not None,
                    }
                )

            return JSONResponse(content={"calls": calls, "total": len(calls)})

        @self.app.post("/webhook/status")
        async def twilio_status_webhook(request: Request):
            """Handle Twilio status webhooks for call events"""
            try:
                # Parse form data from Twilio
                form_data = await request.form()
                call_sid = form_data.get("CallSid")
                call_status = form_data.get("CallStatus")
                from_number = form_data.get("From")
                to_number = form_data.get("To")

                if self.ten_env:
                    self.ten_env.log_info(
                        f"Received status webhook: CallSid={call_sid}, Status={call_status}"
                    )

                # Update call status if we have this call in our sessions
                if call_sid and call_sid in self.active_call_sessions:
                    self.active_call_sessions[call_sid]["status"] = call_status

                    # Log the status change
                    if self.ten_env:
                        self.ten_env.log_info(
                            f"Call {call_sid} status updated to: {call_status}"
                        )

                    # Clean up completed calls
                    if call_status in [
                        "completed",
                        "failed",
                        "busy",
                        "no-answer",
                        "canceled",
                    ]:
                        asyncio.create_task(
                            self._cleanup_call_after_delay(call_sid, 10)
                        )

                # Return TwiML response (empty for status callbacks)
                twiml_response = VoiceResponse()
                return Response(
                    content=str(twiml_response), media_type="application/xml"
                )

            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(
                        f"Error processing status webhook: {e}"
                    )
                return Response(
                    content="<Response></Response>",
                    media_type="application/xml",
                )

        @self.app.get("/health")
        async def health_check():
            """Health check endpoint"""
            return JSONResponse(
                content={
                    "status": "healthy",
                    "active_calls": len(self.active_call_sessions),
                }
            )

    async def _forward_audio_to_ten(self, audio_payload: str, call_sid: str):
        """Forward audio data to TEN framework"""
        try:
            if not self.ten_env:
                return

            # Decode base64 audio data
            audio_data = base64.b64decode(audio_payload)

            # Create AudioFrame and send to TEN framework
            from ten_runtime import AudioFrame

            audio_frame = AudioFrame.create("pcm_frame")
            audio_frame.alloc_buf(len(audio_data))
            buf = audio_frame.lock_buf()
            buf[:] = audio_data
            audio_frame.unlock_buf(buf)
            audio_frame.set_sample_rate(8000)
            audio_frame.set_number_of_channels(1)
            audio_frame.set_bytes_per_sample(2)

            audio_frame.set_property_int("stream_id", 54321)

            audio_frame.set_dests(
                [
                    Loc(
                        app_uri="",
                        graph_id="",
                        extension_name="streamid_adapter",
                    )
                ]
            )

            await self.ten_env.send_audio_frame(audio_frame)

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to forward audio to TEN: {e}")

    async def send_audio_to_twilio(self, audio_data: bytes, call_sid: str):
        """Send audio data to Twilio via WebSocket"""
        try:
            if call_sid not in self.active_call_sessions:
                return

            websocket = self.active_call_sessions[call_sid].get("websocket")
            if not websocket:
                return

            # Encode audio data to base64
            audio_base64 = base64.b64encode(audio_data).decode("utf-8")

            message = {
                "event": "media",
                "streamSid": call_sid,
                "media": {"payload": audio_base64},
            }

            await websocket.send_text(json.dumps(message))

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to send audio to Twilio: {e}")

    async def _cleanup_call_after_delay(
        self, call_sid: str, delay_seconds: int
    ):
        """Clean up call session after a delay"""
        await asyncio.sleep(delay_seconds)
        if call_sid in self.active_call_sessions:
            del self.active_call_sessions[call_sid]
            if self.ten_env:
                self.ten_env.log_info(f"Cleaned up call session: {call_sid}")

    async def start_server(self):
        """Start the FastAPI server"""
        config = uvicorn.Config(
            self.app,
            host="0.0.0.0",
            port=self.config.twilio_server_webhook_http_port,
            log_level="info",
        )
        server = uvicorn.Server(config)
        await server.serve()

    async def start_websocket_server(self):
        """Start the WebSocket server"""
        import websockets

        async def websocket_handler(websocket):
            """Handle WebSocket connections for Twilio media streaming"""
            stream_sid = None
            call_sid = None

            if self.ten_env:
                self.ten_env.log_info(
                    "WebSocket connection established with Twilio"
                )

            try:
                # Wait for start event (may receive connected event first)
                while True:
                    message = await websocket.recv()
                    message = json.loads(message)

                    if message.get("event") == "connected":
                        if self.ten_env:
                            self.ten_env.log_info(
                                f"WebSocket connected: {message}"
                            )
                        continue  # Wait for start event

                    elif "start" in message:
                        stream_sid = message["start"]["streamSid"]
                        call_sid = message["start"]["callSid"]

                        if self.ten_env:
                            self.ten_env.log_info(
                                f"Received start event for streamSid: {stream_sid}, callSid: {call_sid}"
                            )

                        # Store WebSocket connection in active session
                        if call_sid in self.active_call_sessions:
                            self.active_call_sessions[call_sid][
                                "websocket"
                            ] = websocket
                        else:
                            if self.ten_env:
                                self.ten_env.log_warn(
                                    f"Call SID {call_sid} not found in active sessions."
                                )
                        break  # Exit the start event waiting loop

                    else:
                        if self.ten_env:
                            self.ten_env.log_warn(
                                f"Received unexpected event while waiting for start: {message}"
                            )
                        return

                # Process incoming messages after start event
                while True:
                    message = await websocket.recv()
                    message = json.loads(message)

                    if message["event"] == "media":
                        # Handle incoming audio data
                        payload = message["media"]["payload"]
                        if self.ten_env:
                            self.ten_env.log_debug(
                                f"Received audio data: {len(payload)} bytes"
                            )

                        # Forward audio to TEN framework
                        await self._forward_audio_to_ten(payload, call_sid)

                    elif message["event"] == "stop":
                        if self.ten_env:
                            self.ten_env.log_info(
                                f"Received stop event for stream {stream_sid}"
                            )
                        break

                    elif message["event"] == "mark":
                        if self.ten_env:
                            self.ten_env.log_info(
                                f"Received mark event for stream {stream_sid}: {message['mark']['name']}"
                            )

                    else:
                        if self.ten_env:
                            self.ten_env.log_info(
                                f"Received unknown event: {message['event']}"
                            )

            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"WebSocket error: {e}")
            finally:
                if self.ten_env:
                    if stream_sid:
                        self.ten_env.log_info(
                            f"WebSocket connection closed for stream {stream_sid}"
                        )
                    else:
                        self.ten_env.log_info("WebSocket connection closed")
                # Clean up WebSocket reference
                for (
                    session_call_sid,
                    session,
                ) in self.active_call_sessions.items():
                    if session.get("websocket") == websocket:
                        session["websocket"] = None
                        break

        # Use configured WebSocket port
        websocket_port = self.config.twilio_server_media_ws_port
        server = await websockets.serve(
            websocket_handler, "0.0.0.0", websocket_port
        )

        if self.ten_env:
            self.ten_env.log_info(
                f"WebSocket server started on port {websocket_port}"
            )

        await server.wait_closed()
