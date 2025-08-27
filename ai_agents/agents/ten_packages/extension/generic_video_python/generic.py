import base64
import json
import os
import uuid
import asyncio
import requests
import websockets
import threading

from time import time
from agora_token_builder import RtcTokenBuilder

from ten import AsyncTenEnv


class AgoraGenericRecorder:
    SESSION_CACHE_PATH = "/tmp/generic_session_id.txt"

    def __init__(self, app_id: str, app_cert: str, api_key: str, channel_name: str, avatar_uid: int, ten_env: AsyncTenEnv, avatar_id: str, quality: str, version: str, video_encoding: str, enable_string_uid: bool, start_endpoint: str = "https://api.example.com/v1/sessions/start", stop_endpoint: str = "https://api.example.com/v1/sessions/stop"):
        if not app_id or not api_key:
            raise ValueError("AGORA_APP_ID, AGORA_APP_CERT, and API_KEY must be provided.")

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

        self.token_server = self._generate_token(self.uid_avatar, 1)

        self.headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "x-api-key": self.api_key,  # Send api_key in header for security
        }
        self.session_headers = None
        self.session_id = None
        self.realtime_endpoint = None
        self.websocket = None
        self.websocket_task = None
        self._should_reconnect = True

        self._speak_end_timer_task: asyncio.Task | None = None
        self._speak_end_event = asyncio.Event()

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
            with open(self.SESSION_CACHE_PATH, "r") as f:
                return f.read().strip()
        return None

    def _save_session_id(self, session_id: str):
        with open(self.SESSION_CACHE_PATH, "w") as f:
            f.write(session_id)

    def _clear_session_id_cache(self):
        if os.path.exists(self.SESSION_CACHE_PATH):
            os.remove(self.SESSION_CACHE_PATH)

    async def connect(self):
        # Check and stop old session if needed
        old_session_id = self._load_cached_session_id()
        if old_session_id:
            try:
                self.ten_env.log_info(f"Found previous session id: {old_session_id}, attempting to stop it.")
                await self._stop_session(old_session_id)
                self.ten_env.log_info("Previous session stopped.")
                self._clear_session_id_cache()
            except Exception as e:
                self.ten_env.log_error(f"Failed to stop old session: {e}")

        await self._create_session()
        self._save_session_id(self.session_id)
        self.websocket_task = asyncio.create_task(self._connect_websocket_loop())

    async def disconnect(self):
        self._should_reconnect = False
        if self.websocket_task:
            self.websocket_task.cancel()
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                pass
        await self._stop_session(self.session_id)

    async def _create_session(self):
        payload = {
            "avatar_id": self.avatar_id,
            "quality": self.quality,
            "version": self.version,
            "video_encoding": self.video_encoding,
            "agora_settings": {
                "app_id": self.app_id,
                "token": self.token_server,
                "channel": self.channel_name,
                "uid": str(self.uid_avatar),
                "enable_string_uid": self.enable_string_uid
            }
        }

        # Log the request details using existing logging mechanism
        self.ten_env.log_info("Creating new session with details:")
        self.ten_env.log_info(f"URL: {self.start_endpoint}")
        self.ten_env.log_info(f"Headers: {json.dumps(self.headers, indent=2)}")
        self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")

        response = requests.post(self.start_endpoint, json=payload, headers=self.headers)
        self._raise_for_status_verbose(response)
        data = response.json()
        self.session_id = data["session_id"]
        self.realtime_endpoint = data["websocket_address"]
        self.session_token = data["session_token"]

        # Set up session headers with the received token
        self.session_headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "authorization": f"Bearer {self.session_token}"
        }

    def _raise_for_status_verbose(self, response):
        try:
            response.raise_for_status()
        except requests.HTTPError as e:
            self.ten_env.log_error(f"HTTP error response: {response.text}")
            raise e

    async def _stop_session(self, session_id: str):
        try:
            payload = {"session_id": session_id, "session_token": self.session_token}
            self.ten_env.log_info("_stop_session with details:")
            self.ten_env.log_info(f"URL: {self.stop_endpoint}")
            self.ten_env.log_info(f"Headers: {json.dumps(self.headers, indent=2)}")
            self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")
            response = requests.post(self.stop_endpoint, json=payload, headers=self.headers)
            self._raise_for_status_verbose(response)
            self._clear_session_id_cache()
        except Exception as e:
            print(f"Failed to stop session: {e}")

    async def _connect_websocket_loop(self):
        while self._should_reconnect:
            try:
                self.ten_env.log_info("Connecting to WebSocket...")
                async with websockets.connect(self.realtime_endpoint) as ws:
                    self.websocket = ws

                    # Send initial configuration payload with init command
                    initial_payload = {
                        "command": "init",  # Added missing command field as per documentation
                        "avatar_id": self.avatar_id,
                        "quality": self.quality,
                        "version": self.version,
                        "video_encoding": self.video_encoding,
                        "agora_settings": {
                            "app_id": self.app_id,
                            "token": self.token_server,
                            "channel": self.channel_name,
                            "uid": str(self.uid_avatar),
                            "enable_string_uid": self.enable_string_uid
                        }
                    }

                    await self.websocket.send(json.dumps(initial_payload))
                    self.ten_env.log_info("Sent initial configuration payload")

                    # Start listening for messages
                    asyncio.create_task(self._listen_for_messages())

                    await asyncio.Future()  # Wait forever unless cancelled
            except Exception as e:
                print(f"WebSocket error: {e}. Reconnecting in 3 seconds...")
                await asyncio.sleep(3)

    async def _listen_for_messages(self):
        """Listen for incoming WebSocket messages"""
        try:
            async for message in self.websocket:
                data = json.loads(message)
                self.ten_env.log_info(f"Received WebSocket message: {data}")
        except Exception as e:
            self.ten_env.log_error(f"Error listening to WebSocket messages: {e}")

    def _schedule_speak_end(self):
        """Restart debounce timer every time this is called."""
        self._speak_end_event.set()  # signal to reset timer

        if self._speak_end_timer_task is None or self._speak_end_timer_task.done():
            self._speak_end_event = asyncio.Event()
            self._speak_end_timer_task = asyncio.create_task(self._debounced_speak_end())

    async def _debounced_speak_end(self):
        while True:
            try:
                await asyncio.wait_for(self._speak_end_event.wait(), timeout=0.5)
                # Reset the event and loop again
                self._speak_end_event.clear()
            except asyncio.TimeoutError:
                # 500ms passed with no reset — now send end message
                end_evt_id = str(uuid.uuid4())

                # Use the generic message format
                end_message = {
                    "command": "voice_end",
                    "event_id": end_evt_id
                }
                await self.websocket.send(json.dumps(end_message))
                self.ten_env.log_info("Sent voice_end command.")
                break  # Exit the task
            except Exception as e:
                print(f"Error in speak_end task: {e}")
                break

    async def interrupt(self) -> bool:
        """Send voice_interrupt command to the service."""
        if self.websocket is None:
            self.ten_env.log_error("Cannot send interrupt: WebSocket not connected")
            return False

        # Cancel any pending speak_end timer
        if self._speak_end_timer_task and not self._speak_end_timer_task.done():
            self._speak_end_timer_task.cancel()
            self._speak_end_timer_task = None

        try:
            interrupt_msg = {
                "command": "voice_interrupt",
                "event_id": str(uuid.uuid4())
            }
            await self.websocket.send(json.dumps(interrupt_msg))
            self.ten_env.log_info("Sent voice_interrupt command")
            return True
        except Exception as e:
            self.ten_env.log_error(f"Failed to send voice_interrupt command: {e}")
            return False

    async def send(self, audio_base64: str):
        if self.websocket is None:
            raise RuntimeError("WebSocket is not connected.")

        event_id = uuid.uuid4().hex

        # Use the message format from websocket_audio_sender.py
        msg = {
            "command": "voice",
            "audio": audio_base64,
            "sampleRate": 24000,
            "encoding": "PCM16",
            "event_id": event_id
        }

        # Send with retry logic
        for attempt in range(3):
            try:
                await self.websocket.send(json.dumps(msg))
                self.ten_env.log_info(f"Sent audio chunk, event_id: {event_id}")
                break
            except Exception as e:
                if attempt == 2:
                    self.ten_env.log_error(f"Failed to send audio chunk after 3 attempts: {e}")
                    raise
                else:
                    await asyncio.sleep(0.01)

        # Schedule voice_end after a short delay
        self._schedule_speak_end()

    def ws_connected(self):
        return self.websocket is not None