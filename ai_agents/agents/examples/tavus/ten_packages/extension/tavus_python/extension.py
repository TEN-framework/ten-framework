import asyncio
import aiohttp
import json
import time
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

        # Persona creation configuration
        self.auto_create_persona: bool = False
        self.persona_name: str = ""
        self.system_prompt: str = ""
        self.context: str = ""
        self.enable_perception: bool = False
        self.perception_model: str = "raven-0"
        self.enable_smart_turn_detection: bool = True
        self.auto_start_on_boot: bool = False
        self.llm_provider: str = ""
        self.llm_model: str = ""
        self.llm_base_url: str = ""
        self.llm_api_key: str = ""
        self.tts_provider: str = ""
        self.tts_voice_id: str = ""

        # Conversation state
        self.conversation_id: Optional[str] = None
        self.conversation_url: Optional[str] = None
        self.is_active: bool = False
        self.created_persona_id: Optional[str] = None

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
            self.replica_id = await ten_env.get_property_string("replica_id")
            self.conversation_name = await ten_env.get_property_string("conversation_name")

            # Optional properties with defaults
            try:
                self.persona_id = await ten_env.get_property_string("persona_id")
            except:
                self.persona_id = ""

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

            # Persona creation configuration
            try:
                self.auto_create_persona = await ten_env.get_property_bool("auto_create_persona")
            except:
                self.auto_create_persona = False

            try:
                self.persona_name = await ten_env.get_property_string("persona_name")
            except:
                self.persona_name = ""

            try:
                self.system_prompt = await ten_env.get_property_string("system_prompt")
            except:
                self.system_prompt = ""

            try:
                self.context = await ten_env.get_property_string("context")
            except:
                self.context = ""

            try:
                self.enable_perception = await ten_env.get_property_bool("enable_perception")
            except:
                self.enable_perception = False

            try:
                self.perception_model = await ten_env.get_property_string("perception_model")
            except:
                self.perception_model = "raven-0"

            try:
                self.enable_smart_turn_detection = await ten_env.get_property_bool("enable_smart_turn_detection")
            except:
                self.enable_smart_turn_detection = True

            try:
                self.auto_start_on_boot = await ten_env.get_property_bool("auto_start_on_boot")
            except:
                self.auto_start_on_boot = False

            try:
                self.llm_provider = await ten_env.get_property_string("llm_provider")
            except:
                self.llm_provider = ""

            try:
                self.llm_model = await ten_env.get_property_string("llm_model")
            except:
                self.llm_model = ""

            try:
                self.llm_base_url = await ten_env.get_property_string("llm_base_url")
            except:
                self.llm_base_url = ""

            try:
                self.llm_api_key = await ten_env.get_property_string("llm_api_key")
            except:
                self.llm_api_key = ""

            try:
                self.tts_provider = await ten_env.get_property_string("tts_provider")
            except:
                self.tts_provider = ""

            try:
                self.tts_voice_id = await ten_env.get_property_string("tts_voice_id")
            except:
                self.tts_voice_id = ""

            # Create HTTP session
            self.session = aiohttp.ClientSession(
                headers={
                    "x-api-key": self.api_key,
                    "Content-Type": "application/json"
                }
            )

            ten_env.log_info(f"Tavus configuration loaded: persona_id={self.persona_id}, auto_create_persona={self.auto_create_persona}")

            # Create persona if auto_create_persona is enabled
            if self.auto_create_persona and not self.persona_id:
                await self._create_persona(ten_env)

            # Optionally auto start the conversation once initialization completes
            if self.auto_start_on_boot:
                ten_env.log_info("Auto-start on boot is enabled; scheduling conversation start.")
                asyncio.create_task(self._auto_start_conversation(ten_env))

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

    async def _create_persona(self, ten_env: AsyncTenEnv):
        """Create a Tavus persona using the full pipeline."""
        if not self.session:
            ten_env.log_error("HTTP session not initialized")
            return

        try:
            # Build layers configuration
            layers = {}

            # Perception layer (screen sharing capabilities)
            if self.enable_perception:
                layers["perception"] = {
                    "model": self.perception_model
                }

            # STT layer (Speech-to-Text with smart turn detection)
            if self.enable_smart_turn_detection:
                layers["stt"] = {
                    "smart_endpointing": True
                }

            # LLM layer (Language Model)
            if self.llm_provider:
                llm_config = {
                    "provider": self.llm_provider
                }
                if self.llm_model:
                    llm_config["model"] = self.llm_model
                if self.llm_base_url:
                    llm_config["base_url"] = self.llm_base_url
                if self.llm_api_key:
                    llm_config["api_key"] = self.llm_api_key
                layers["llm"] = llm_config

            # TTS layer (Text-to-Speech)
            if self.tts_provider:
                tts_config = {
                    "provider": self.tts_provider
                }
                if self.tts_voice_id:
                    tts_config["voice_id"] = self.tts_voice_id
                layers["tts"] = tts_config

            # Create persona payload
            payload = {
                "persona_name": self.persona_name or "TEN Framework Persona",
                "system_prompt": self.system_prompt or "You are a helpful AI assistant.",
                "default_replica_id": self.replica_id,
                "pipeline_mode": "full"
            }

            if self.context:
                payload["context"] = self.context

            if layers:
                payload["layers"] = layers

            ten_env.log_info(f"Creating Tavus persona with payload: {json.dumps(payload, indent=2)}")

            async with self.session.post(
                "https://tavusapi.com/v2/personas",
                json=payload
            ) as response:
                if response.status == 200 or response.status == 201:
                    result = await response.json()
                    self.created_persona_id = result.get("persona_id")
                    self.persona_id = self.created_persona_id

                    ten_env.log_info(f"Tavus persona created successfully: {self.persona_id}")

                    # Send persona created event
                    data = Data.create("tavus_persona_created")
                    data.set_property_string("persona_id", self.persona_id)
                    await ten_env.send_data(data)
                    await self._broadcast_text_event(
                        ten_env,
                        "persona_created",
                        {
                            "persona_id": self.persona_id,
                            "replica_id": self.replica_id,
                            "auto_created": True
                        }
                    )
                else:
                    error_text = await response.text()
                    ten_env.log_error(f"Failed to create Tavus persona: {response.status} - {error_text}")

        except Exception as e:
            ten_env.log_error(f"Error creating Tavus persona: {e}")

    async def _start_conversation(self, ten_env: AsyncTenEnv):
        """Start a Tavus conversation."""
        if self.is_active:
            ten_env.log_warn("Conversation already active")
            return

        if not self.session:
            ten_env.log_error("HTTP session not initialized")
            return

        # Make sure we have a persona_id
        if not self.persona_id:
            ten_env.log_error("No persona_id available. Set persona_id or enable auto_create_persona.")
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
                    await self._broadcast_text_event(
                        ten_env,
                        "conversation_created",
                        {
                            "conversation_id": self.conversation_id,
                            "conversation_url": self.conversation_url,
                            "persona_id": self.persona_id,
                            "replica_id": self.replica_id
                        }
                    )
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
            ended_conversation_id = self.conversation_id
            ended_conversation_url = self.conversation_url
            if self.conversation_id and self.session:
                # Tavus conversations auto-terminate based on timeout settings
                # Optionally, you could call a DELETE endpoint if available
                self.ten_env.log_info(f"Ending Tavus conversation: {self.conversation_id}")

            self.is_active = False
            self.conversation_id = None
            self.conversation_url = None

            if self.ten_env and ended_conversation_id:
                await self._broadcast_text_event(
                    self.ten_env,
                    "conversation_ended",
                    {
                        "conversation_id": ended_conversation_id,
                        "conversation_url": ended_conversation_url,
                        "persona_id": self.persona_id,
                        "replica_id": self.replica_id
                    }
                )

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Error ending Tavus conversation: {e}")

    async def _broadcast_text_event(self, ten_env: AsyncTenEnv, event: str, payload: dict):
        """Broadcast Tavus lifecycle events over text_data so websocket clients can react."""
        try:
            message = {
                "data_type": "tavus_event",
                "event": event,
                "timestamp_ms": int(time.time() * 1000),
                "payload": payload,
            }
            data = Data.create("text_data")
            data.set_property_from_json(None, json.dumps(message))
            await ten_env.send_data(data)
        except Exception as e:
            ten_env.log_error(f"Failed to broadcast Tavus event {event}: {e}")

    async def _auto_start_conversation(self, ten_env: AsyncTenEnv):
        """Ensure auto-start waits for persona configuration when enabled."""
        # Small delay to allow runtime to finish other startup tasks
        await asyncio.sleep(0.1)
        await self._start_conversation(ten_env)
