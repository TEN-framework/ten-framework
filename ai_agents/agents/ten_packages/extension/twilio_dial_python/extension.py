#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
import asyncio
from typing import Any, Dict

from ten_runtime import Cmd, CmdResult, StatusCode
from ten_runtime.async_ten_env import AsyncTenEnv
from pydantic import BaseModel, Field
from ten_ai_base.llm_tool import AsyncLLMToolBaseExtension
from ten_ai_base.types import (
    LLMToolMetadata,
    LLMToolMetadataParameter,
    LLMToolResult,
    LLMToolResultLLMResult,
)

# Twilio SDK
from twilio.rest import Client
from twilio.twiml import VoiceResponse

# Command constants
CMD_OUT_MAKE_CALL = "make_call"
CMD_PROPERTY_PHONE_NUMBER = "phone_number"
CMD_PROPERTY_MESSAGE = "message"
CMD_PROPERTY_WEBHOOK_URL = "webhook_url"

# Tool constants
TOOL_NAME = "make_outbound_call"
TOOL_DESCRIPTION = "Make an outbound call to a phone number with a message"


class TwilioConfig(BaseModel):
    account_sid: str = Field(default="", description="Twilio Account SID")
    auth_token: str = Field(default="", description="Twilio Auth Token")
    from_number: str = Field(default="", description="Twilio phone number to call from")


class TwilioDialExtension(AsyncLLMToolBaseExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.config: TwilioConfig | None = None
        self.twilio_client: Client | None = None
        self.ten_env: AsyncTenEnv | None = None

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
            await super().on_start(ten_env)
        except Exception as e:
            ten_env.log_error(f"Failed to initialize Twilio client: {e}")
            self.twilio_client = None

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_stop")
        await super().on_stop(ten_env)

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("TwilioDialExtension on_deinit")
        await super().on_deinit(ten_env)

    def get_tool_metadata(self, ten_env: AsyncTenEnv) -> list[LLMToolMetadata]:
        return [
            LLMToolMetadata(
                name=TOOL_NAME,
                description=TOOL_DESCRIPTION,
                parameters=[
                    LLMToolMetadataParameter(
                        name="phone_number",
                        type="string",
                        description="The phone number to call (e.g., +1234567890)",
                        required=True,
                    ),
                    LLMToolMetadataParameter(
                        name="message",
                        type="string",
                        description="The message to speak during the call",
                        required=True,
                    ),
                ],
            ),
        ]

    async def run_tool(
        self, ten_env: AsyncTenEnv, name: str, args: dict
    ) -> LLMToolResult | None:
        ten_env.log_info(f"TwilioDialExtension run_tool: {name}, args: {args}")

        if name == TOOL_NAME:
            phone_number = args.get("phone_number")
            message = args.get("message")

            if not phone_number or not message:
                return LLMToolResultLLMResult(
                    type="llmresult",
                    content=json.dumps({"error": "phone_number and message are required"}),
                )

            result = await self._make_outbound_call(phone_number, message, ten_env)
            return LLMToolResultLLMResult(
                type="llmresult",
                content=json.dumps(result),
            )

        return None

    async def _make_outbound_call(
        self, phone_number: str, message: str, ten_env: AsyncTenEnv
    ) -> Dict[str, Any]:
        """Make an outbound call using Twilio"""
        try:
            if not self.twilio_client:
                return {"error": "Twilio client not initialized"}

            # Create TwiML response for the call
            twiml_response = VoiceResponse()
            twiml_response.say(message, voice='alice')
            twiml_response.hangup()

            # Make the call
            call = self.twilio_client.calls.create(
                to=phone_number,
                from_=self.config.from_number,
                twiml=str(twiml_response)
            )

            ten_env.log_info(f"Call initiated: SID={call.sid}, To={phone_number}, From={self.config.from_number}")

            # Send cmd_out to notify the system about the call
            await self._send_make_call_cmd(ten_env, phone_number, message, call.sid)

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

    async def _send_make_call_cmd(
        self, ten_env: AsyncTenEnv, phone_number: str, message: str, call_sid: str
    ) -> None:
        """Send cmd_out to notify the system about the outbound call"""
        try:
            cmd = Cmd.create(CMD_OUT_MAKE_CALL)
            cmd.set_property_string(CMD_PROPERTY_PHONE_NUMBER, phone_number)
            cmd.set_property_string(CMD_PROPERTY_MESSAGE, message)
            cmd.set_property_string("call_sid", call_sid)

            ten_env.log_info(f"Sending cmd_out: {CMD_OUT_MAKE_CALL}")
            await ten_env.send_cmd(cmd)

        except Exception as e:
            ten_env.log_error(f"Failed to send make_call cmd: {e}")
