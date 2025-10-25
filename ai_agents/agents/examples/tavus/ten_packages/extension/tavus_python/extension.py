import asyncio
import aiohttp
import json
from typing import Optional
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    AudioFrame,
    VideoFrame,
)


class TavusExtension(AsyncExtension):
    """
    Tavus Conversational Video Interface Extension for TEN Framework.
    Manages Tavus persona conversations and bridges with Agora RTC.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: Optional[AsyncTenEnv] = None

        # Tavus configuration
        self.api_key: str = ""
        self.persona_id: str = ""
        self.replica_id: str = ""
        self.conversation_name: str = ""
        self.max_call_duration: int = 3600
        self.enable_recording: bool = False
        self.language: str = "en"

        # Conversation state
        self.conversation_id: Optional[str] = None
        self.conversation_url: Optional[str] = None
        self.is_active: bool = False

        # HTTP session
        self.session: Optional[aiohttp.ClientSession] = None

    async def on_init(self, ten_env: AsyncTenEnv):
        """Initialize the extension."""
        self.ten_env = ten_env
        ten_env.log_info("TavusExtension on_init")

    async def on_start(self, ten_env: AsyncTenEnv):
        """Start the extension and load configuration."""
        ten_env.log_info("TavusExtension on_start")

        # Load configuration from property
        try:
            self.api_key = await ten_env.get_property_string("tavus_api_key")
            self.persona_id = await ten_env.get_property_string("persona_id")
            self.replica_id = await ten_env.get_property_string("replica_id")
            self.conversation_name = await ten_env.get_property_string("conversation_name")

            # Optional properties
            try:
                self.max_call_duration = await ten_env.get_property_int("max_call_duration")
            except:
                self.max_call_duration = 3600

            try:
                self.enable_recording = await ten_env.get_property_bool("enable_recording")
            except:
                self.enable_recording = False

            try:
                self.language = await ten_env.get_property_string("language")
            except:
                self.language = "en"

            # Create HTTP session
            self.session = aiohttp.ClientSession(
                headers={
                    "x-api-key": self.api_key,
                    "Content-Type": "application/json"
                }
            )

            ten_env.log_info(f"Tavus configuration loaded: persona_id={self.persona_id}")

        except Exception as e:
            ten_env.log_error(f"Failed to load Tavus configuration: {e}")

    async def on_stop(self, ten_env: AsyncTenEnv):
        """Stop the extension and cleanup resources."""
        ten_env.log_info("TavusExtension on_stop")

        # Close conversation if active
        if self.is_active and self.conversation_id:
            await self._end_conversation()

        # Close HTTP session
        if self.session:
            await self.session.close()
            self.session = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        """Handle commands."""
        cmd_name = cmd.get_name()
        ten_env.log_info(f"TavusExtension received cmd: {cmd_name}")

        if cmd_name == "on_user_joined":
            await self._handle_user_joined(ten_env, cmd)
        elif cmd_name == "on_user_left":
            await self._handle_user_left(ten_env, cmd)
        elif cmd_name == "start_conversation":
            await self._handle_start_conversation(ten_env, cmd)
        elif cmd_name == "end_conversation":
            await self._handle_end_conversation(ten_env, cmd)
        else:
            ten_env.log_warn(f"Unknown command: {cmd_name}")
            await ten_env.return_result(Cmd.create("result"), cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        """Handle data messages."""
        data_name = data.get_name()
        ten_env.log_debug(f"TavusExtension received data: {data_name}")

        # Forward data to Tavus if conversation is active
        if self.is_active:
            # Handle any data forwarding to Tavus conversation
            pass

    async def on_audio_frame(self, ten_env: AsyncTenEnv, audio_frame: AudioFrame):
        """Handle audio frames from Agora RTC."""
        if self.is_active:
            # Forward audio to Tavus conversation
            # Note: Tavus uses its own WebRTC infrastructure via Daily.co
            # Audio bridging would require additional integration
            pass

    async def on_video_frame(self, ten_env: AsyncTenEnv, video_frame: VideoFrame):
        """Handle video frames from Agora RTC."""
        if self.is_active:
            # Forward video to Tavus conversation
            # Note: Tavus uses its own WebRTC infrastructure via Daily.co
            # Video bridging would require additional integration
            pass

    async def _handle_user_joined(self, ten_env: AsyncTenEnv, cmd: Cmd):
        """Handle user joined event."""
        ten_env.log_info("User joined, starting Tavus conversation")

        # Automatically start Tavus conversation when user joins
        await self._start_conversation(ten_env)

        await ten_env.return_result(Cmd.create("result"), cmd)

    async def _handle_user_left(self, ten_env: AsyncTenEnv, cmd: Cmd):
        """Handle user left event."""
        ten_env.log_info("User left, ending Tavus conversation")

        # Automatically end Tavus conversation when user leaves
        await self._end_conversation()

        await ten_env.return_result(Cmd.create("result"), cmd)

    async def _handle_start_conversation(self, ten_env: AsyncTenEnv, cmd: Cmd):
        """Handle start conversation command."""
        await self._start_conversation(ten_env)
        await ten_env.return_result(Cmd.create("result"), cmd)

    async def _handle_end_conversation(self, ten_env: AsyncTenEnv, cmd: Cmd):
        """Handle end conversation command."""
        await self._end_conversation()
        await ten_env.return_result(Cmd.create("result"), cmd)

    async def _start_conversation(self, ten_env: AsyncTenEnv):
        """Start a Tavus conversation."""
        if self.is_active:
            ten_env.log_warn("Conversation already active")
            return

        if not self.session:
            ten_env.log_error("HTTP session not initialized")
            return

        try:
            # Create conversation via Tavus API
            payload = {
                "replica_id": self.replica_id,
                "persona_id": self.persona_id,
                "conversation_name": self.conversation_name,
                "properties": {
                    "max_call_duration": self.max_call_duration,
                    "enable_recording": self.enable_recording,
                    "language": self.language
                }
            }

            ten_env.log_info(f"Creating Tavus conversation with payload: {json.dumps(payload)}")

            async with self.session.post(
                "https://tavusapi.com/v2/conversations",
                json=payload
            ) as response:
                if response.status == 200 or response.status == 201:
                    result = await response.json()
                    self.conversation_id = result.get("conversation_id")
                    self.conversation_url = result.get("conversation_url")
                    self.is_active = True

                    ten_env.log_info(f"Tavus conversation created: {self.conversation_id}")
                    ten_env.log_info(f"Conversation URL: {self.conversation_url}")

                    # Send conversation URL as data to frontend
                    data = Data.create("tavus_conversation_created")
                    data.set_property_string("conversation_id", self.conversation_id)
                    data.set_property_string("conversation_url", self.conversation_url)
                    await ten_env.send_data(data)
                else:
                    error_text = await response.text()
                    ten_env.log_error(f"Failed to create Tavus conversation: {response.status} - {error_text}")

        except Exception as e:
            ten_env.log_error(f"Error starting Tavus conversation: {e}")

    async def _end_conversation(self):
        """End the Tavus conversation."""
        if not self.is_active:
            return

        try:
            if self.conversation_id and self.session:
                # Tavus conversations auto-terminate based on timeout settings
                # Optionally, you could call a DELETE endpoint if available
                self.ten_env.log_info(f"Ending Tavus conversation: {self.conversation_id}")

            self.is_active = False
            self.conversation_id = None
            self.conversation_url = None

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error ending Tavus conversation: {e}")
