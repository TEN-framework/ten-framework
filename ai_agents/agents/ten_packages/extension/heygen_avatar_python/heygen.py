import json
import os
import uuid
import asyncio
import requests
import websockets
from time import time
from agora_token_builder import RtcTokenBuilder

from ten_runtime import AsyncTenEnv  # pylint: disable=import-error


class AgoraHeygenRecorder:
    SESSION_CACHE_PATH = "/tmp/heygen_session_id.txt"

    def __init__(
        self,
        app_id: str,
        app_cert: str,
        heygen_api_key: str,
        channel_name: str,
        avatar_uid: int,
        avatar_name: str,
        ten_env: AsyncTenEnv,
    ):
        if not app_id or not heygen_api_key:
            raise ValueError(
                "AGORA_APP_ID, AGORA_APP_CERT, and HEYGEN_API_KEY must be provided."
            )

        self.app_id = app_id
        self.app_cert = app_cert
        self.api_key = heygen_api_key
        self.channel_name = channel_name
        self.uid_avatar = avatar_uid
        self.avatar_name = avatar_name
        self.ten_env = ten_env

        self.token_server = self._generate_token(self.uid_avatar, 1)

        self.headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "x-api-key": self.api_key,
        }
        self.session_token = None
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
            with open(self.SESSION_CACHE_PATH, "r", encoding="utf-8") as f:
                return f.read().strip()
        return None

    def _save_session_id(self, session_id: str):
        with open(self.SESSION_CACHE_PATH, "w", encoding="utf-8") as f:
            f.write(session_id)

    def _clear_session_id_cache(self):
        if os.path.exists(self.SESSION_CACHE_PATH):
            os.remove(self.SESSION_CACHE_PATH)

    async def connect(self):
        await self._create_token()

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

        await self._create_session()
        await self._start_session()
        self._save_session_id(self.session_id)
        self.websocket_task = asyncio.create_task(
            self._connect_websocket_loop()
        )

    async def disconnect(self):
        self.ten_env.log_info(
            "[HEYGEN DISCONNECT] Starting disconnect sequence"
        )
        self._should_reconnect = False
        if self.websocket_task:
            self.ten_env.log_info(
                "[HEYGEN DISCONNECT] Cancelling websocket task"
            )
            self.websocket_task.cancel()
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                self.ten_env.log_info(
                    "[HEYGEN DISCONNECT] Websocket task cancelled"
                )
                pass
        self.ten_env.log_info(
            f"[HEYGEN DISCONNECT] Stopping session: {self.session_id}"
        )
        await self._stop_session(self.session_id)
        self.ten_env.log_info("[HEYGEN DISCONNECT] Disconnect completed")

    async def _create_token(self):
        response = requests.post(
            "https://api.heygen.com/v1/streaming.create_token",
            json={},
            headers=self.headers,
            timeout=30,
        )
        self._raise_for_status_verbose(response)
        self.session_token = response.json()["data"]["token"]
        self.session_headers = {
            "accept": "application/json",
            "content-type": "application/json",
            "authorization": f"Bearer {self.session_token}",
        }

    async def _create_session(self):
        payload = {
            "avatar_name": self.avatar_name,
            "quality": "high",
            "version": "agora_v1",
            "video_encoding": "H264",
            "source": "app",
            "disable_idle_timeout": False,
            "agora_settings": {
                "app_id": self.app_id,
                "token": self.token_server,
                "channel": self.channel_name,
                "uid": str(self.uid_avatar),
            },
            "namespace": "demo",
        }

        # Log the request details using existing logging mechanism
        self.ten_env.log_info("Creating new session with details:")
        self.ten_env.log_info("URL: https://api.heygen.com/v1/streaming.new")
        self.ten_env.log_info(
            f"Headers: {json.dumps(self.session_headers, indent=2)}"
        )
        self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")

        response = requests.post(
            "https://api.heygen.com/v1/streaming.new",
            json=payload,
            headers=self.session_headers,
            timeout=30,
        )
        self._raise_for_status_verbose(response)
        data = response.json()["data"]
        self.session_id = data["session_id"]
        self.realtime_endpoint = data["realtime_endpoint"]
        self.ten_env.log_info(f"Session created: {self.session_id}")
        self.ten_env.log_info(f"WebSocket endpoint: {self.realtime_endpoint}")

    def _raise_for_status_verbose(self, response):
        try:
            response.raise_for_status()
        except requests.HTTPError as e:
            self.ten_env.log_error(f"HTTP error response: {response.text}")
            raise e

    async def _start_session(self):
        payload = {"session_id": self.session_id}
        self.ten_env.log_info(f"Starting session with payload: {payload}")
        response = requests.post(
            "https://api.heygen.com/v1/streaming.start",
            json=payload,
            headers=self.session_headers,
            timeout=30,
        )
        self._raise_for_status_verbose(response)

    async def _stop_session(self, session_id: str):
        try:
            payload = {"session_id": session_id}
            self.ten_env.log_info("_stop_session with details:")
            self.ten_env.log_info(
                "URL: https://api.heygen.com/v1/streaming.stop"
            )
            self.ten_env.log_info(
                f"Headers: {json.dumps(self.headers, indent=2)}"
            )
            self.ten_env.log_info(f"Payload: {json.dumps(payload, indent=2)}")
            response = requests.post(
                "https://api.heygen.com/v1/streaming.stop",
                json=payload,
                headers=self.headers,
                timeout=30,
            )
            self._raise_for_status_verbose(response)
            self._clear_session_id_cache()
        except Exception as e:
            self.ten_env.log_error(f"Failed to stop session: {e}")

    async def _send_keep_alive(self):
        """Send periodic keep_alive messages to maintain HeyGen connection"""
        while self._should_reconnect and self.websocket:
            try:
                await asyncio.sleep(10)  # Send keep_alive every 10 seconds
                if self.websocket and not self.websocket.closed:
                    # Use new session.keep_alive format with event_id
                    event_id = f"keepalive_{int(time.time() * 1000)}"
                    keep_alive_msg = json.dumps(
                        {"type": "session.keep_alive", "event_id": event_id}
                    )
                    await self.websocket.send(keep_alive_msg)
                    self.ten_env.log_debug(
                        f"[avatar] Sent session.keep_alive (event_id={event_id})"
                    )
            except Exception as e:
                self.ten_env.log_error(f"[avatar] Keep-alive error: {e}")
                break

    async def _connect_websocket_loop(self):
        while self._should_reconnect:
            try:
                self.ten_env.log_info(
                    f"Connecting to WebSocket: {self.realtime_endpoint}"
                )
                async with websockets.connect(
                    self.realtime_endpoint, ping_interval=20, ping_timeout=60
                ) as ws:
                    self.websocket = ws
                    self.ten_env.log_info(
                        "WebSocket connected successfully with ping_interval=20s, ping_timeout=60s"
                    )

                    # Start keep_alive task
                    keep_alive_task = asyncio.create_task(
                        self._send_keep_alive()
                    )

                    try:
                        # Consume messages from HeyGen to prevent buffer overflow
                        async for message in ws:
                            self.ten_env.log_debug(
                                f"[avatar] Received message from HeyGen: {message[:100]}"
                            )
                    finally:
                        keep_alive_task.cancel()
                        try:
                            await keep_alive_task
                        except asyncio.CancelledError:
                            pass
            except Exception as e:
                self.ten_env.log_error(
                    f"WebSocket error: {e}. Reconnecting in 3 seconds..."
                )
                self.websocket = None
                await asyncio.sleep(3)

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
                    self._speak_end_event.wait(), timeout=0.5
                )
                # Reset the event and loop again
                self._speak_end_event.clear()
            except asyncio.TimeoutError:
                # 500ms passed with no reset — now send
                end_evt_id = str(uuid.uuid4())
                await self.websocket.send(
                    json.dumps(
                        {"type": "agent.speak_end", "event_id": end_evt_id}
                    )
                )
                self.ten_env.log_info("Sent agent.speak_end.")
                break  # Exit the task
            except Exception as e:
                self.ten_env.log_error(f"Error in speak_end task: {e}")
                break

    async def send(self, audio_base64: str):
        if self.websocket is None:
            self.ten_env.log_error(
                "Cannot send audio: WebSocket is not connected"
            )
            raise RuntimeError("WebSocket is not connected.")

        event_id = uuid.uuid4().hex
        audio_len = len(audio_base64)
        self.ten_env.log_debug(
            f"Sending audio chunk: {audio_len} bytes, event_id: {event_id}"
        )

        try:
            await self.websocket.send(
                json.dumps(
                    {
                        "type": "agent.speak",
                        "audio": audio_base64,
                        "event_id": event_id,
                    }
                )
            )
            self.ten_env.log_debug(f"Audio chunk sent successfully: {event_id}")
        except Exception as e:
            self.ten_env.log_error(f"Failed to send audio chunk: {e}")
            raise

        # Schedule agent.speak_end after a short delay
        self._schedule_speak_end()

    async def interrupt(self) -> bool:
        """Send agent.interrupt message to HeyGen service to stop current speech."""
        # Cancel any pending speak_end timer
        if self._speak_end_timer_task and not self._speak_end_timer_task.done():
            self._speak_end_timer_task.cancel()
            self._speak_end_timer_task = None

        success = await self._send_message(
            {"type": "agent.interrupt", "event_id": uuid.uuid4().hex}
        )

        if success:
            self.ten_env.log_debug("Sent agent.interrupt")
        else:
            self.ten_env.log_error("Failed to send interrupt message")

        return success

    async def _send_message(self, message: dict) -> bool:
        """Send a message to HeyGen WebSocket without waiting for confirmation."""
        if self.websocket is None:
            self.ten_env.log_error(
                "Cannot send message: WebSocket not connected"
            )
            return False

        try:
            await self.websocket.send(json.dumps(message))
            self.ten_env.log_debug(
                f"Sent message: {message.get('type', 'unknown')}"
            )
            return True
        except Exception as e:
            self.ten_env.log_error(f"Failed to send message: {e}")
            return False

    def ws_connected(self):
        return self.websocket is not None
