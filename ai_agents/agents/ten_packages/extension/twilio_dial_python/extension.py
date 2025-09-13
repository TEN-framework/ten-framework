#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
import asyncio
import websockets
import base64
from typing import Any, Dict, Optional

from ten_runtime import Cmd, CmdResult, StatusCode, AsyncExtension, Data, AudioFrame
from ten_runtime.async_ten_env import AsyncTenEnv
from pydantic import BaseModel, Field

# Twilio SDK
from twilio.rest import Client
from twilio.twiml.voice_response import VoiceResponse

# Command constants
CMD_PROPERTY_PHONE_NUMBER = "phone_number"
CMD_PROPERTY_MESSAGE = "message"


class TwilioConfig(BaseModel):
    account_sid: str = Field(default="", description="Twilio Account SID")
    auth_token: str = Field(default="", description="Twilio Auth Token")
    from_number: str = Field(default="", description="Twilio phone number to call from")
    webhook_url: str = Field(default="", description="Webhook URL for Twilio to connect to")


class TwilioDialExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: TwilioConfig | None = None
        self.twilio_client: Client | None = None
        self.ten_env: AsyncTenEnv | None = None
        self.websocket: Optional[websockets.WebSocketServerProtocol] = None
        self.current_call_sid: Optional[str] = None
        self.audio_queue: asyncio.Queue[bytes] = asyncio.Queue()

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_init")
        await super().on_init(ten_env)

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_start")
        self.ten_env = ten_env

        # Load configuration using Pydantic
        config_json, _ = await ten_env.get_property_to_json()
        try:
            self.config = TwilioConfig.model_validate_json(config_json)
            ten_env.log_info(f"Twilio config loaded: account_sid={self.config.account_sid[:10]}...")
        except Exception as e:
            ten_env.log_error(f"Failed to load Twilio config: {e}")
            self.config = TwilioConfig()  # Use default empty config

        if not self.config.account_sid or not self.config.auth_token:
            ten_env.log_error("Twilio credentials not configured. Please set TWILIO_ACCOUNT_SID and TWILIO_AUTH_TOKEN environment variables.")
            return

        if not self.config.from_number:
            ten_env.log_error("Twilio from_number not configured. Please set TWILIO_FROM_NUMBER in property.json")
            return

        # Initialize Twilio client
        try:
            self.twilio_client = Client(self.config.account_sid, self.config.auth_token)
            ten_env.log_info("Twilio client initialized successfully")
        except Exception as e:
            ten_env.log_error(f"Failed to initialize Twilio client: {e}")
            self.twilio_client = None

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_stop")
        await super().on_stop(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_deinit")
        await super().on_deinit(ten_env)

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        """Handle incoming commands"""
        cmd_name = cmd.get_name()
        ten_env.log_info(f"TwilioDialExtension received cmd: {cmd_name}")

        if cmd_name == "make_call":
            # Handle make_call command
            phone_number, _ = cmd.get_property_string(CMD_PROPERTY_PHONE_NUMBER)
            message, _ = cmd.get_property_string(CMD_PROPERTY_MESSAGE)

            if not phone_number or not message:
                ten_env.log_error("make_call cmd missing required properties: phone_number and message")
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd, "Missing required properties: phone_number and message")
                await ten_env.return_result(cmd_result)
                return

            result = await self._make_outbound_call(phone_number, message, ten_env)
            ten_env.log_info(f"Call result: {result}")

            # Return success result with call information
            if result.get("success"):
                cmd_result = CmdResult.create(StatusCode.OK, cmd)
                cmd_result.set_property_string("call_sid", result.get("call_sid", ""))
                cmd_result.set_property_string("phone_number", result.get("phone_number", ""))
                cmd_result.set_property_string("status", result.get("status", ""))
                await ten_env.return_result(cmd_result)
            else:
                error_msg = result.get("error", "Unknown error occurred")
                cmd_result = CmdResult.create(StatusCode.ERROR, cmd, error_msg)
                await ten_env.return_result(cmd_result)
        else:
            ten_env.log_warn(f"Unknown command: {cmd_name}")
            cmd_result = CmdResult.create(StatusCode.ERROR, cmd, f"Unknown command: {cmd_name}")
            await ten_env.return_result(cmd_result)

    async def _make_outbound_call(
        self, phone_number: str, message: str, ten_env: AsyncTenEnv
    ) -> Dict[str, Any]:
        """Make an outbound call using Twilio with WebSocket streaming"""
        try:
            if not self.twilio_client:
                return {"error": "Twilio client not initialized"}

            if not self.config.webhook_url:
                return {"error": "Webhook URL not configured"}

            # Create TwiML response for the call with WebSocket streaming
            twiml_response = VoiceResponse()
            twiml_response.say(message, voice='alice')
            # Add WebSocket streaming
            twiml_response.start().stream(url=f"wss://{self.config.webhook_url}/ws")
            twiml_response.pause(length=30)  # Keep call open for 30 seconds

            # Make the call
            call = self.twilio_client.calls.create(
                to=phone_number,
                from_=self.config.from_number,
                twiml=str(twiml_response)
            )

            self.current_call_sid = call.sid
            ten_env.log_info(f"Call initiated: SID={call.sid}, To={phone_number}, From={self.config.from_number}")

            # Start WebSocket server for audio streaming
            await self._start_websocket_server(ten_env)

            return {
                "success": True,
                "call_sid": call.sid,
                "phone_number": phone_number,
                "message": message,
                "status": call.status
            }

        except Exception as e:
            ten_env.log_error(f"Failed to make outbound call: {e}")
            return {
                "error": str(e),
                "phone_number": phone_number,
                "message": message
            }

    async def _start_websocket_server(self, ten_env: AsyncTenEnv):
        """Start WebSocket server for Twilio audio streaming"""
        try:
            # Start WebSocket server in background
            asyncio.create_task(self._websocket_handler(ten_env))
            ten_env.log_info("WebSocket server started for audio streaming")
        except Exception as e:
            ten_env.log_error(f"Failed to start WebSocket server: {e}")

    async def _websocket_handler(self, ten_env: AsyncTenEnv):
        """Handle WebSocket connections from Twilio"""
        try:
            # This would typically be handled by the FastAPI server
            # For now, we'll simulate the WebSocket connection
            ten_env.log_info("WebSocket handler started")

            # Start audio processing task
            asyncio.create_task(self._process_audio_stream(ten_env))

        except Exception as e:
            ten_env.log_error(f"WebSocket handler error: {e}")

    async def _process_audio_stream(self, ten_env: AsyncTenEnv):
        """Process incoming audio stream from Twilio"""
        try:
            while self.current_call_sid:
                # Simulate receiving audio data from Twilio
                # In real implementation, this would come from WebSocket
                await asyncio.sleep(0.1)  # Simulate audio chunks

                # Process audio data and send to TEN framework
                # This would be real audio data from Twilio
                audio_data = b"fake_audio_data"  # Replace with real audio

                # Create audio frame and send to TEN framework
                audio_frame = AudioFrame.create("audio_in")
                audio_frame.set_buf(audio_data)
                audio_frame.set_sample_rate(8000)  # Twilio uses 8kHz
                audio_frame.set_channels(1)  # Mono
                audio_frame.set_sample_width(2)  # 16-bit

                await ten_env.send_audio_frame(audio_frame)

        except Exception as e:
            ten_env.log_error(f"Audio stream processing error: {e}")

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame) -> None:
        """Handle outgoing audio frames from TEN framework"""
        try:
            if self.websocket and self.current_call_sid:
                # Get audio data from frame
                audio_data = audio_frame.get_buf()

                # Convert to base64 for Twilio WebSocket
                audio_base64 = base64.b64encode(audio_data).decode('utf-8')

                # Send to Twilio via WebSocket
                message = {
                    "event": "media",
                    "streamSid": self.current_call_sid,
                    "media": {
                        "payload": audio_base64
                    }
                }

                await self.websocket.send(json.dumps(message))
                ten_env.log_debug(f"Sent audio frame to Twilio: {len(audio_data)} bytes")

        except Exception as e:
            ten_env.log_error(f"Failed to send audio frame to Twilio: {e}")

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        """Handle data messages"""
        data_name = data.get_name()
        ten_env.log_debug(f"Received data: {data_name}")

        # Handle different types of data messages
        if data_name == "text_data":
            # Handle text data from ASR
            text, _ = data.get_property_string("text")
            is_final, _ = data.get_property_bool("is_final")
            ten_env.log_info(f"Received text: {text} (final: {is_final})")

            # Process text and generate response
            # This would typically involve LLM processing
            response_text = f"AI Response to: {text}"

            # Send response back as audio (would go through TTS)
            response_data = Data.create("tts_request")
            response_data.set_property_string("text", response_text)
            await ten_env.send_data(response_data)
