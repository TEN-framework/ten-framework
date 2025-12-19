import json
import os
import uuid
import asyncio
import requests
import websockets
import random

from time import time
from agora_token_builder import RtcTokenBuilder

from ten_runtime import AsyncTenEnv

# Error codes
ERROR_CODE_FAILED_TO_SEND_AUDIO_DATA = -1001
ERROR_CODE_REALTIME_ENDPOINT_NOT_SET = -1002
ERROR_CODE_MAX_RECONNECTION_ATTEMPTS_REACHED = -1003
ERROR_CODE_FAILED_TO_PARSE_WEBSOCKET_MESSAGE = -1004
ERROR_CODE_FAILED_TO_CREATE_SESSION = -1007
ERROR_CODE_FAILED_TO_CONNECT_TO_SERVICE = -1008
ERROR_CODE_FAILED_TO_STOP_SESSION = -1009
ERROR_CODE_FAILED_TO_SEND_INTERRUPT_MESSAGE = -1010
ERROR_CODE_CONFIG_VALIDATION_ERROR = -1012
ERROR_CODE_REQUEST_HTTP_ERROR = -1006

# Constants
HEARTBEAT_INTERVAL = 10  # seconds - WebSocket heartbeat interval
MAX_RECONNECT_ATTEMPTS = 15  # Stop after ~1 minute of attempts
RECONNECT_DELAY = 1  # seconds
SPEAK_END_TIMEOUT = 0.5  # seconds


class AgoraGenericRecorder:
    SESSION_CACHE_PATH = "/tmp/generic_session_id.txt"

    def __init__(
        self,
        app_id: str,
        app_cert: str,
        api_key: str,
        channel_name: str,
        avatar_uid: int,
        ten_env: AsyncTenEnv,
        avatar_id: str,
        quality: str,
        version: str,
        video_encoding: str,
        enable_string_uid: bool,
        start_endpoint: str,
        stop_endpoint: str,
        activity_idle_timeout: int,
    ):
        # Validate required fields
        self._validate_config(app_id, api_key, avatar_id)

        self.app_id = app_id
        self.app_cert = app_cert
        self.api_key = api_key
        self.channel_name = channel_name
        self.uid_avatar = avatar_uid
        self.ten_env = ten_env
        self.avatar_id = avatar_id
        self.quality = quality
        self.version = version
        self.video_encoding = video_encoding
        self.enable_string_uid = enable_string_uid
        self.start_endpoint = start_endpoint
        self.stop_endpoint = stop_endpoint
        self.activity_idle_timeout = activity_idle_timeout

        self.token_server = self._generate_token(self.uid_avatar, 1)

        self.headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "x-api-key": self.api_key,
        }
        self.session_id = None
        self.session_token = None
        self.realtime_endpoint = None
        self.websocket = None
        self.websocket_task = None
        self.heartbeat_task = None
        self._should_reconnect = True
        self._connection_broken = False  # Flag to trigger reconnection

        self._speak_end_timer_task: asyncio.Task | None = None
        self._speak_end_event = asyncio.Event()

    def _validate_config(self, app_id: str, api_key: str, avatar_id: str):
        """Validate required configuration parameters."""
        required_fields = {
            "app_id": app_id,
            "api_key": api_key,
            "avatar_id": avatar_id,
        }

        for field_name, value in required_fields.items():
            if not value or (isinstance(value, str) and value.strip() == ""):
                raise ValueError(
                    f"Required field is missing or empty: {field_name}"
                )

    def _generate_token(self, uid, role):
        # if the app_cert is not required, return an empty string
        if not self.app_cert:
            return self.app_id

        expire_time = 3600
        privilege_expired_ts = int(time()) + expire_time
        return RtcTokenBuilder.buildTokenWithUid(
            self.app_id,
            self.app_cert,
            self.channel_name,
            uid,
            role,
            privilege_expired_ts,
        )

    def _load_cached_session_id(self):
        if os.path.exists(self.SESSION_CACHE_PATH):
            with open(self.SESSION_CACHE_PATH, "r", encoding="utf-8") as f:
                return f.read().strip()
        return None

    def _save_session_id(self, session_id: str):
        with open(self.SESSION_CACHE_PATH, "w", encoding="utf-8") as f:
            f.write(session_id)

    def _clear_session_id_cache(self):
        if os.path.exists(self.SESSION_CACHE_PATH):
            os.remove(self.SESSION_CACHE_PATH)

    def get_connection_status(self) -> dict[str, any]:
        """Get current connection status and information."""
        return {
            "connected": self.websocket is not None,
            "session_id": self.session_id,
            "realtime_endpoint": self.realtime_endpoint,
            "should_reconnect": self._should_reconnect,
        }

    async def connect(self):
        # Check and stop old session if needed
        old_session_id = self._load_cached_session_id()
        if old_session_id:
            try:
                self.ten_env.log_info(
                    f"Found previous session id: {old_session_id}, attempting to stop it."
                )
                await self._stop_session(old_session_id)
                self.ten_env.log_info("Previous session stopped.")
                self._clear_session_id_cache()
            except Exception as e:
                self.ten_env.log_error(f"Failed to stop old session: {e}")

        try:
            await self._create_session()
            self._save_session_id(self.session_id)

            # Start WebSocket connection
            self.websocket_task = asyncio.create_task(
                self._connect_websocket_loop()
            )

            # Start heartbeat task
            self.heartbeat_task = asyncio.create_task(self._start_heartbeat())

        except Exception as e:
            await self._handle_error(
                f"Failed to connect to service: {e}",
                code=ERROR_CODE_FAILED_TO_CONNECT_TO_SERVICE,
            )
            raise

    async def disconnect(self):
        self.ten_env.log_info("Starting disconnection from service")
        self._should_reconnect = False

        # Cancel heartbeat task
        if self.heartbeat_task:
            self.ten_env.log_info("Cancelling heartbeat task")
            self.heartbeat_task.cancel()
            try:
                await self.heartbeat_task
            except asyncio.CancelledError:
                self.ten_env.log_info("Heartbeat task cancelled successfully")
            except Exception as e:
                self.ten_env.log_error(
                    f"Error while cancelling heartbeat task: {e}"
                )

        # Cancel WebSocket task
        if self.websocket_task:
            self.ten_env.log_info("Cancelling WebSocket task")
            self.websocket_task.cancel()
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                self.ten_env.log_info("WebSocket task cancelled successfully")
            except Exception as e:
                self.ten_env.log_error(
                    f"Error while cancelling WebSocket task: {e}"
                )

        # Stop session
        if self.session_id:
            try:
                await self._stop_session(self.session_id)
            except Exception as e:
                await self._handle_error(
                    f"Error while stopping session {self.session_id}: {e}",
                    code=ERROR_CODE_FAILED_TO_STOP_SESSION,
                )

        self.ten_env.log_info("Disconnection completed")

    async def _create_session(self):
        payload = {
            "avatar_id": self.avatar_id,
            "quality": self.quality,
            "version": self.version,
            "video_encoding": self.video_encoding,
            "activity_idle_timeout": self.activity_idle_timeout,
            "agora_settings": {
                "app_id": self.app_id,
                "token": self.token_server,
                "channel": self.channel_name,
                "uid": str(self.uid_avatar),
                "enable_string_uid": self.enable_string_uid,
            },
        }

        # Log the request details using existing logging mechanism
        self.ten_env.log_info("Creating new session with details:")
        self.ten_env.log_info(f"URL: {self.start_endpoint}")
        self.ten_env.log_info(f"Headers: {json.dumps(self.headers, indent=2)}")
        self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")

        response = requests.post(
            self.start_endpoint, json=payload, headers=self.headers, timeout=30
        )
        await self._raise_for_status_verbose(response)
        data = response.json()

        # Validate required fields
        if "session_id" not in data:
            raise ValueError("Missing 'session_id' field in response")
        if "websocket_address" not in data:
            raise ValueError("Missing 'websocket_address' field in response")
        if "session_token" not in data:
            raise ValueError("Missing 'session_token' field in response")

        self.session_id = data["session_id"]
        self.realtime_endpoint = data["websocket_address"]
        self.session_token = data["session_token"]

    async def _raise_for_status_verbose(self, response):
        try:
            response.raise_for_status()
        except requests.HTTPError as e:
            # Try to parse JSON error response
            error_details = f"HTTP {response.status_code} Error: {e}"
            try:
                if response.headers.get("content-type", "").startswith(
                    "application/json"
                ):
                    error_data = response.json()
                    if "message" in error_data:
                        error_details += (
                            f". Server message: {error_data['message']}"
                        )
                    if "code" in error_data:
                        error_details += f". Error code: {error_data['code']}"
                else:
                    error_details += f". Response Body: {response.text}"
            except (ValueError, KeyError, TypeError):
                error_details += f". Response Body: {response.text}"

            await self._handle_error(
                error_details, code=ERROR_CODE_REQUEST_HTTP_ERROR
            )
            raise

    async def _stop_session(self, session_id: str):
        try:
            # Payload contains only session_id
            payload = {"session_id": session_id}

            # Add session token to headers for authentication
            headers = self.headers.copy()
            if self.session_token:
                headers["authorization"] = f"Bearer {self.session_token}"

            self.ten_env.log_info("_stop_session with details:")
            self.ten_env.log_info(f"URL: {self.stop_endpoint}")
            self.ten_env.log_info(
                f"Headers: {json.dumps({k: v for k, v in headers.items() if k != 'authorization'}, indent=2)}"
            )
            self.ten_env.log_info("Authorization: Bearer ***masked***")
            self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")

            # Use DELETE method as specified in API documentation
            response = requests.delete(
                self.stop_endpoint, json=payload, headers=headers, timeout=30
            )
            await self._raise_for_status_verbose(response)
            self._clear_session_id_cache()
        except Exception as e:
            self.ten_env.log_error(f"Failed to stop session: {e}")
            raise

    async def _start_heartbeat(self) -> None:
        """Start periodic WebSocket heartbeat to maintain connection."""
        while self._should_reconnect:
            try:
                await asyncio.sleep(HEARTBEAT_INTERVAL)
                if self.websocket is not None:
                    heartbeat_msg = {
                        "command": "heartbeat",
                        "event_id": str(uuid.uuid4()),
                        "timestamp": int(time() * 1000),
                    }
                    await self.websocket.send(json.dumps(heartbeat_msg))
                    self.ten_env.log_debug("Sent WebSocket heartbeat")
            except asyncio.CancelledError:
                break
            except Exception as e:
                self.ten_env.log_error(f"Heartbeat error: {e}")
                # Mark connection as broken to trigger reconnection
                self._connection_broken = True

    async def _connect_websocket_loop(self):
        attempt = 0
        while self._should_reconnect:
            try:
                if not self.realtime_endpoint:
                    await self._handle_error(
                        "realtime_endpoint is not set",
                        code=ERROR_CODE_REALTIME_ENDPOINT_NOT_SET,
                    )
                    return

                self.ten_env.log_info(
                    f"Connecting to WebSocket at {self.realtime_endpoint} (attempt {attempt + 1})"
                )

                # Prepare WebSocket headers with session token (same as test scripts)
                headers = {}
                if self.session_token:
                    headers["authorization"] = f"Bearer {self.session_token}"

                # Use additional_headers for WebSocket authentication (websockets 10.4)
                async with websockets.connect(
                    self.realtime_endpoint, additional_headers=headers
                ) as websocket:
                    self.websocket = websocket
                    self._connection_broken = False  # Reset broken flag
                    attempt = (
                        0  # Reset attempt counter on successful connection
                    )
                    self.ten_env.log_info(
                        "WebSocket connected successfully with headers"
                    )

                    # Send initial configuration payload with init command
                    initial_payload = {
                        "command": "init",
                        "session_id": self.session_id,
                        "avatar_id": self.avatar_id,
                        "quality": self.quality,
                        "version": self.version,
                        "video_encoding": self.video_encoding,
                        "activity_idle_timeout": self.activity_idle_timeout,
                        "agora_settings": {
                            "app_id": self.app_id,
                            "token": self.token_server,
                            "channel": self.channel_name,
                            "uid": str(self.uid_avatar),
                            "enable_string_uid": self.enable_string_uid,
                        },
                    }

                    await self.websocket.send(json.dumps(initial_payload))
                    self.ten_env.log_info("Sent initial configuration payload")

                    # Start listening for messages
                    asyncio.create_task(self._listen_for_messages())

                    # Wait for connection to be broken or cancelled
                    while (
                        not self._connection_broken and self._should_reconnect
                    ):
                        await asyncio.sleep(1)

            except websockets.exceptions.ConnectionClosed as e:
                attempt += 1
                await self._handle_connection_error(e, attempt)
            except Exception as e:
                attempt += 1
                await self._handle_connection_error(e, attempt)
            finally:
                self.websocket = None

    async def _handle_connection_error(
        self, error: Exception, attempt: int
    ) -> None:
        """Handle WebSocket connection errors with exponential backoff."""
        base_delay = RECONNECT_DELAY

        # Check for media server connection error (like HeyGen)
        if "Failed to connect to media server" in str(
            error
        ) or "Connection refused" in str(error):
            self.ten_env.log_error(
                f"Connection error detected: {error}. Initiating full reconnection..."
            )
            # For serious connection errors, wait longer before retrying
            await asyncio.sleep(8)
            return

        # Check if we should stop retrying
        if MAX_RECONNECT_ATTEMPTS != -1 and attempt >= MAX_RECONNECT_ATTEMPTS:
            await self._handle_error(
                f"Max reconnection attempts ({MAX_RECONNECT_ATTEMPTS}) reached. Stopping reconnection.",
                code=ERROR_CODE_MAX_RECONNECTION_ATTEMPTS_REACHED,
            )
            self._should_reconnect = False
            return

        # After 3 failed attempts, try creating a new session
        if attempt >= 3:
            self.ten_env.log_info(
                f"Multiple connection failures (attempt {attempt}). Creating new session..."
            )
            try:
                # Clear old session cache
                self._clear_session_id_cache()

                # Create a new session
                await self._create_session()
                self._save_session_id(self.session_id)

                self.ten_env.log_info(f"New session created: {self.session_id}")
                # Continue with normal delay logic instead of immediate retry

            except Exception as session_error:
                self.ten_env.log_error(
                    f"Failed to create new session: {session_error}"
                )

                # If session creation fails multiple times, increase delay significantly
                if attempt >= 6:  # After 3 session creation attempts
                    actual_delay = 30  # Wait 30 seconds before trying again
                    self.ten_env.log_error(
                        f"Multiple session creation failures. Waiting {actual_delay}s before retry."
                    )
                    await asyncio.sleep(actual_delay)
                    return

        # Delay for first 5 attempts, then exponential backoff with jitter
        if attempt < 5:
            actual_delay = base_delay
        else:
            delay = min(base_delay * (2 ** (attempt - 5)), 10)
            jitter = random.uniform(0.8, 1.2)  # Add 20% jitter
            actual_delay = delay * jitter

        # Show attempt info
        attempt_info = f"attempt {attempt}"
        if MAX_RECONNECT_ATTEMPTS != -1:
            attempt_info += f"/{MAX_RECONNECT_ATTEMPTS}"
        else:
            attempt_info += " (unlimited)"

        self.ten_env.log_error(
            f"WebSocket connection failed ({attempt_info}): {error}. "
            f"Retrying in {actual_delay:.1f} seconds..."
        )
        await asyncio.sleep(actual_delay)

    async def _listen_for_messages(self):
        """Listen for incoming WebSocket messages"""
        try:
            async for message in self.websocket:
                # Convert message to string if it's bytes
                if isinstance(message, bytes):
                    message = message.decode("utf-8")
                await self._handle_websocket_message(message)
        except Exception as e:
            self.ten_env.log_error(
                f"Error listening to WebSocket messages: {e}"
            )
            # Mark connection as broken to trigger reconnection
            self._connection_broken = True

    async def _handle_websocket_message(self, message: str) -> None:
        """Handle incoming WebSocket messages."""
        try:
            data = json.loads(message)
            self.ten_env.log_debug(f"Received WebSocket message: {data}")

        except json.JSONDecodeError as e:
            await self._handle_error(
                f"Failed to parse WebSocket message: {e}",
                code=ERROR_CODE_FAILED_TO_PARSE_WEBSOCKET_MESSAGE,
            )

    def _schedule_speak_end(self):
        """Restart debounce timer every time this is called."""
        self._speak_end_event.set()  # signal to reset timer

        if (
            self._speak_end_timer_task is None
            or self._speak_end_timer_task.done()
        ):
            self._speak_end_event = asyncio.Event()
            self._speak_end_timer_task = asyncio.create_task(
                self._debounced_speak_end()
            )

    async def _debounced_speak_end(self):
        while True:
            try:
                await asyncio.wait_for(
                    self._speak_end_event.wait(), timeout=SPEAK_END_TIMEOUT
                )
                # Reset the event and loop again
                self._speak_end_event.clear()
            except asyncio.TimeoutError:
                # 500ms passed with no reset — now send end message
                end_evt_id = str(uuid.uuid4())

                # Use the generic message format
                end_message = {"command": "voice_end", "event_id": end_evt_id}
                success = await self._send_message(end_message)
                if success:
                    self.ten_env.log_info("Sent voice_end command.")
                break  # Exit the task
            except Exception as e:
                self.ten_env.log_error(f"Error in speak_end task: {e}")
                break

    async def interrupt(self) -> bool:
        """Send voice_interrupt command to the service."""
        if self.websocket is None:
            self.ten_env.log_error(
                "Cannot send interrupt: WebSocket not connected"
            )
            return False

        # Cancel any pending speak_end timer
        if self._speak_end_timer_task and not self._speak_end_timer_task.done():
            self._speak_end_timer_task.cancel()
            self._speak_end_timer_task = None

        interrupt_msg = {
            "command": "voice_interrupt",
            "event_id": str(uuid.uuid4()),
        }

        success = await self._send_message(interrupt_msg)
        if success:
            self.ten_env.log_info("Sent voice_interrupt command")
        else:
            await self._handle_error(
                "Failed to send interrupt message",
                code=ERROR_CODE_FAILED_TO_SEND_INTERRUPT_MESSAGE,
            )

        return success

    async def send_voice_end(self) -> bool:
        """
        Send voice_end message immediately.
        Called when tts_audio_end (reason=1) is received, indicating TTS generation complete.
        """
        # Cancel any pending debounce timer
        if self._speak_end_timer_task and not self._speak_end_timer_task.done():
            self._speak_end_timer_task.cancel()
            self._speak_end_timer_task = None

        end_message = {"command": "voice_end", "event_id": str(uuid.uuid4())}
        success = await self._send_message(end_message)

        if success:
            self.ten_env.log_info(
                "[GENERIC] Sent voice_end (triggered by tts_audio_end reason=1)"
            )
        else:
            self.ten_env.log_error("Failed to send voice_end message")

        return success

    async def send(self, audio_base64: str, sample_rate: int = 24000):
        if self.websocket is None:
            await self._handle_error(
                "Cannot send audio: WebSocket not connected",
                code=ERROR_CODE_FAILED_TO_SEND_AUDIO_DATA,
            )
            raise RuntimeError("WebSocket is not connected.")

        event_id = uuid.uuid4().hex

        # Use the message format from websocket_audio_sender.py
        msg = {
            "command": "voice",
            "audio": audio_base64,
            "sampleRate": sample_rate,  # Use the actual sample rate passed in
            "encoding": "PCM16",
            "event_id": event_id,
        }

        success = await self._send_message(msg)

        if success:
            self.ten_env.log_info(f"Sent audio chunk, event_id: {event_id}")
            # NOTE: voice_end is now triggered by tts_audio_end event
            # instead of 500ms debounce timer. See send_voice_end() method.
        else:
            await self._handle_error(
                f"Failed to send audio chunk: {event_id}",
                code=ERROR_CODE_FAILED_TO_SEND_AUDIO_DATA,
            )
            raise RuntimeError("Failed to send audio data")

    async def _send_message(self, message: dict) -> bool:
        """Send a message without waiting for confirmation."""
        if self.websocket is None:
            return False

        try:
            await self.websocket.send(json.dumps(message))
            self.ten_env.log_debug(
                f"Sent message: {message.get('command', 'unknown')}"
            )
            return True
        except Exception as e:
            self.ten_env.log_error(f"Failed to send message: {e}")
            # Mark connection as broken to trigger reconnection
            self._connection_broken = True
            return False

    async def _handle_error(self, message: str, code: int = 0) -> None:
        """Handle and log errors consistently."""
        self.ten_env.log_error(f"Error {code}: {message}")

        # Send structured error message to system
        try:
            from ten_ai_base import ErrorMessage
            from ten_runtime import Data

            data = Data.create("message")
            error_msg = ErrorMessage(
                module="avatar",
                message=message,
                code=code,
            )
            data.set_property_from_json("", error_msg.model_dump_json())
            asyncio.create_task(self.ten_env.send_data(data))
        except ImportError:
            # Fall back to simple logging if ErrorMessage not available
            pass

    def ws_connected(self):
        try:
            return (
                self.websocket is not None
                and hasattr(self.websocket, "state")
                and self.websocket.state.name == "OPEN"
            )
        except (AttributeError, RuntimeError):
            return False
