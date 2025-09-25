#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
import json
import logging
import os
import random
import shutil
import socket
import subprocess
import time
from datetime import datetime
from pathlib import Path
from typing import Dict, Any, Optional
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse, Response
import uvicorn
from fastapi.middleware.cors import CORSMiddleware

# Twilio imports
from twilio.rest import Client
from twilio.twiml.voice_response import VoiceResponse

from pydantic import BaseModel, Field


class TwilioServerConfig(BaseModel):
    # Twilio configuration
    twilio_account_sid: str = Field(
        default="", description="Twilio Account SID"
    )
    twilio_auth_token: str = Field(default="", description="Twilio Auth Token")
    twilio_from_number: str = Field(
        default="", description="Twilio phone number to call from"
    )

    # Server webhook configuration
    twilio_server_webhook_http_port: int = Field(
        default=8000, description="HTTP port for server webhook endpoints"
    )


class TwilioServer:
    """FastAPI server for Twilio integration"""

    def __init__(self, config: TwilioServerConfig):
        self.config = config
        self.app = FastAPI(title="Twilio Dial Server", version="1.0.0")
        # Configure CORS to allow frontend to call this API
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],  # For demo; tighten in production
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )
        self.active_call_sessions: Dict[str, Dict[str, Any]] = {}
        self.twilio_client: Optional[Client] = None
        self.logger = logging.getLogger(__name__)
        # property.json moved under tenapp
        self.property_json_path = "./tenapp/property.json"
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
                self.logger.info("Twilio client initialized successfully")
            except Exception as e:
                self.logger.error(f"Failed to initialize Twilio client: {e}")

    def _copy_property_json(self) -> str:
        """Copy property.json to /tmp/ten_agent/sip_<timestamp>_property.json"""
        # Create /tmp/ten_agent directory if it doesn't exist
        tmp_dir = Path("/tmp/ten_agent")
        tmp_dir.mkdir(parents=True, exist_ok=True)

        # Generate timestamp-based filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]  # milliseconds
        new_filename = f"sip_{timestamp}_property.json"
        new_path = tmp_dir / new_filename

        # Copy the file
        shutil.copy2(self.property_json_path, new_path)
        self.logger.info(f"Copied property.json to {new_path}")

        return str(new_path)

    def _assign_websocket_port(self, property_json_path: str) -> int:
        """Assign a random websocket port between 9000-9500 and update the property.json"""
        # Generate random port between 9000-9500
        websocket_port = random.randint(9000, 9500)

        # Read the property.json file
        with open(property_json_path, 'r') as f:
            property_data = json.load(f)

        # Find main_control under ten.predefined_graphs[0].graph.nodes and update websocket port
        if 'ten' in property_data and 'predefined_graphs' in property_data['ten']:
            for graph in property_data['ten']['predefined_graphs']:
                if 'graph' in graph and 'nodes' in graph['graph']:
                    for node in graph['graph']['nodes']:
                        if node.get('name') == 'main_control':
                            # Update the websocket port property
                            if 'property' not in node:
                                node['property'] = {}
                            node['property']['twilio_server_media_ws_port'] = websocket_port
                            self.logger.info(f"Updated websocket port to {websocket_port} for main_control node")
                            break

        # Write the updated property.json back
        with open(property_json_path, 'w') as f:
            json.dump(property_data, f, indent=2)

        return websocket_port

    def _is_port_available(self, port: int) -> bool:
        """Check if a port is available"""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(('localhost', port))
                return True
            except OSError:
                return False

    def _wait_for_port(self, port: int, timeout: int = 30) -> bool:
        """Wait for a port to become available"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            if self._is_port_available(port):
                return True
            time.sleep(0.5)
        return False

    def _spawn_ten_process(self, property_json_path: str) -> subprocess.Popen:
        """Spawn a TEN process with the given property.json"""
        cmd = ["tman", "run", "start", "--", "--property", property_json_path]
        self.logger.info(f"Spawning TEN process: {' '.join(cmd)}")

        # Ensure we run inside the tenapp folder so tman resolves manifest correctly
        tenapp_dir = (Path(__file__).resolve().parent.parent / "tenapp").resolve()
        self.logger.info(f"TEN process working directory: {tenapp_dir}")

        # Respect LOG_STDOUT env: if true, stream directly to terminal (no log file)
        log_stdout_env = os.getenv("LOG_STDOUT", "false").lower() in ("1", "true", "yes", "on")
        if log_stdout_env:
            # Inherit parent's stdout/stderr (default None); also merge stderr to stdout for ordering
            process = subprocess.Popen(
                cmd,
                stdout=None,
                stderr=None,
                text=True,
                cwd=str(tenapp_dir),
            )
            # Mark that no file logging is used
            process.log_file = None
            process.log_path = None
            self.logger.info("TEN process logging to terminal (LOG_STDOUT=true)")
            return process

        # Otherwise, log to a single combined file (stdout and stderr together)
        log_dir = Path("/tmp/ten_agent/logs")
        log_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
        combined_log = log_dir / f"ten_{timestamp}.log"
        self.logger.info(f"TEN process combined log: {combined_log}")

        log_file = open(combined_log, 'w')
        process = subprocess.Popen(
            cmd,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=str(tenapp_dir),
        )

        # Store single log file reference for cleanup
        process.log_file = log_file
        process.log_path = str(combined_log)

        return process

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

                # Step 1: Copy property.json to /tmp/ten_agent/sip_<timestamp>_property.json
                property_json_path = self._copy_property_json()

                # Step 2: Assign random websocket port and update the copied json file
                websocket_port = self._assign_websocket_port(property_json_path)

                # Step 3: Spawn TEN process and wait for websocket port to be available
                ten_process = self._spawn_ten_process(property_json_path)

                # Wait for the websocket port to become available (TEN process to start)
                if not self._wait_for_port(websocket_port, timeout=30):
                    ten_process.terminate()
                    raise HTTPException(
                        status_code=500, detail=f"Failed to start TEN process on port {websocket_port}"
                    )

                self.logger.info(f"TEN process started successfully on port {websocket_port}")

                # Create TwiML response
                twiml_response = VoiceResponse()
                twiml_response.say(message, voice="alice")

                # Add status callback for call events
                # Use the configured HTTP port for status callback
                status_callback_url = f"http://localhost:{self.config.twilio_server_webhook_http_port}/webhook/status"

                # Step 4: Start Twilio client request
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

                # Store call session with TEN process info
                self.active_call_sessions[call.sid] = {
                    "phone_number": phone_number,
                    "message": message,
                    "status": call.status,
                    "call_type": "outbound",
                    "created_at": time.time(),
                    "property_json_path": property_json_path,
                    "websocket_port": websocket_port,
                    "ten_process": ten_process,
                }

                self.logger.info(f"Outbound call initiated: SID={call.sid}, To={phone_number}")

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
                self.logger.error(f"Failed to start call: {e}")
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

                session = self.active_call_sessions[call_sid]

                if self.twilio_client:
                    # Hang up the call via Twilio API
                    call = self.twilio_client.calls(call_sid).update(
                        status="completed"
                    )

                # Clean up TEN process but keep log files
                if "ten_process" in session:
                    ten_process = session["ten_process"]
                    try:
                        # Terminate the process if it's still running
                        if ten_process.poll() is None:
                            ten_process.terminate()
                            ten_process.wait(timeout=5)

                        # Close combined log file handle if present; keep the file itself
                        if hasattr(ten_process, 'log_file') and ten_process.log_file:
                            ten_process.log_file.close()
                            self.logger.info(f"TEN process terminated for call {call_sid}. Log preserved at: {ten_process.log_path}")
                        else:
                            # Backward compatibility: close old split file handles
                            if hasattr(ten_process, 'stdout_file'):
                                ten_process.stdout_file.close()
                            if hasattr(ten_process, 'stderr_file'):
                                ten_process.stderr_file.close()
                            paths = []
                            if hasattr(ten_process, 'stdout_log'):
                                paths.append(str(ten_process.stdout_log))
                            if hasattr(ten_process, 'stderr_log'):
                                paths.append(str(ten_process.stderr_log))
                            if paths:
                                self.logger.info(f"TEN process terminated for call {call_sid}. Logs preserved at: {', '.join(paths)}")
                            else:
                                self.logger.info(f"TEN process terminated for call {call_sid}")
                    except Exception as e:
                        self.logger.error(f"Error cleaning up TEN process for call {call_sid}: {e}")

                # Remove from active sessions
                del self.active_call_sessions[call_sid]

                self.logger.info(f"Call {call_sid} stopped")

                return JSONResponse(
                    content={"message": f"Call {call_sid} stopped"}
                )

            except Exception as e:
                self.logger.error(f"Failed to stop call: {e}")
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

                self.logger.info(f"Received status webhook: CallSid={call_sid}, Status={call_status}")

                # Update call status if we have this call in our sessions
                if call_sid and call_sid in self.active_call_sessions:
                    self.active_call_sessions[call_sid]["status"] = call_status

                    # Log the status change
                    self.logger.info(f"Call {call_sid} status updated to: {call_status}")

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
                self.logger.error(f"Error processing status webhook: {e}")
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

    async def _cleanup_call_after_delay(
        self, call_sid: str, delay_seconds: int
    ):
        """Clean up call session after a delay"""
        await asyncio.sleep(delay_seconds)
        if call_sid in self.active_call_sessions:
            session = self.active_call_sessions[call_sid]

            # Clean up TEN process and log files
            if "ten_process" in session:
                ten_process = session["ten_process"]
                try:
                    # Terminate the process if it's still running
                    if ten_process.poll() is None:
                        ten_process.terminate()
                        ten_process.wait(timeout=5)

                    # Close single combined log file if present (new behavior)
                    if hasattr(ten_process, 'log_file') and ten_process.log_file:
                        ten_process.log_file.close()
                        self.logger.info(f"TEN process terminated and combined log file closed for call {call_sid}: {ten_process.log_path}")
                    else:
                        # Backward compatibility: close old split files if they exist
                        if hasattr(ten_process, 'stdout_file'):
                            ten_process.stdout_file.close()
                        if hasattr(ten_process, 'stderr_file'):
                            ten_process.stderr_file.close()
                        self.logger.info(f"TEN process terminated and legacy split log files closed for call {call_sid}")
                except Exception as e:
                    self.logger.error(f"Error cleaning up TEN process for call {call_sid}: {e}")

            del self.active_call_sessions[call_sid]
            self.logger.info(f"Cleaned up call session: {call_sid}")

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
