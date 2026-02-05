#
# Thymia Sentinel WebSocket Client
# Created by Claude Code in 2025.
#
"""
WebSocket client for Thymia Sentinel real-time analysis service.

This client handles:
- WebSocket connection to wss://ws.thymia.ai
- Audio streaming (user and agent tracks)
- Transcript forwarding
- Receiving policy results and status updates
- Auto-reconnect on disconnection
"""

import asyncio
import json
import time
from typing import Optional, Callable, Union, Awaitable

import traceback

try:
    import websockets
    from websockets.exceptions import (
        ConnectionClosed,
        ConnectionClosedError,
        ConnectionClosedOK,
    )
except ImportError:
    websockets = None

from .sentinel_protocol import (
    SentinelConfig,
    AudioHeader,
    TranscriptMessage,
    PolicyResult,
    StatusMessage,
    ErrorMessage,
)

# Type aliases for callbacks
PolicyResultCallback = Union[
    Callable[[PolicyResult], None],
    Callable[[PolicyResult], Awaitable[None]],
]
StatusCallback = Union[
    Callable[[StatusMessage], None],
    Callable[[StatusMessage], Awaitable[None]],
]
ErrorCallback = Union[
    Callable[[ErrorMessage], None],
    Callable[[ErrorMessage], Awaitable[None]],
]


class SentinelClient:
    """
    WebSocket client for Thymia Sentinel real-time analysis.

    Streams audio and transcripts to the Sentinel server and receives
    policy results with biomarkers and safety classification.

    Example:
        ```python
        async def on_result(result: PolicyResult):
            print(f"Got result: {result.biomarker_summary}")

        client = SentinelClient(
            api_key="your-api-key",
            on_policy_result=on_result,
        )

        config = SentinelConfig(
            api_key="your-api-key",
            user_label="user-123",
            date_of_birth="1990-01-01",
            birth_sex="MALE",
        )

        await client.connect(config)

        # Stream audio
        await client.send_audio(pcm_data, track="user")
        await client.send_transcript("user", "Hello", is_final=True)

        await client.disconnect()
        ```
    """

    DEFAULT_SERVER_URL = "wss://ws.thymia.ai"
    RECONNECT_DELAY_INITIAL = 1.0
    RECONNECT_DELAY_MAX = 30.0
    RECONNECT_DELAY_MULTIPLIER = 2.0

    def __init__(
        self,
        api_key: str,
        server_url: Optional[str] = None,
        on_policy_result: Optional[PolicyResultCallback] = None,
        on_status: Optional[StatusCallback] = None,
        on_error: Optional[ErrorCallback] = None,
        auto_reconnect: bool = True,
        log_callback: Optional[Callable[[str, str], None]] = None,
    ):
        """
        Initialize the Sentinel client.

        Args:
            api_key: Thymia API key for authentication
            server_url: WebSocket server URL (default: wss://ws.thymia.ai)
            on_policy_result: Callback for policy results (biomarkers, safety)
            on_status: Callback for status updates (buffer progress)
            on_error: Callback for error messages
            auto_reconnect: Whether to auto-reconnect on disconnection
            log_callback: Optional callback for logging (level, message)
        """
        if websockets is None:
            raise ImportError(
                "websockets package is required for SentinelClient. "
                "Install with: pip install websockets"
            )

        self.api_key = api_key
        self.server_url = server_url or self.DEFAULT_SERVER_URL
        self.on_policy_result = on_policy_result
        self.on_status = on_status
        self.on_error = on_error
        self.auto_reconnect = auto_reconnect
        self._log = log_callback or self._default_log

        self._websocket: Optional[websockets.WebSocketClientProtocol] = None
        self._receive_task: Optional[asyncio.Task] = None
        self._connected = False
        self._config: Optional[SentinelConfig] = None
        self._should_reconnect = False
        self._reconnect_delay = self.RECONNECT_DELAY_INITIAL
        self._audio_send_lock = asyncio.Lock()

        # Ready event - signals when server acknowledges CONFIG
        self._ready_event: Optional[asyncio.Event] = None

        # Tracking
        self._last_status: Optional[StatusMessage] = None
        self._latest_policy_result: Optional[PolicyResult] = None
        self._results_received_count = 0

    @staticmethod
    def _default_log(level: str, message: str):
        """Default logging function."""
        print(f"[SENTINEL_{level.upper()}] {message}", flush=True)

    def _log_info(self, message: str):
        self._log("info", message)

    def _log_warn(self, message: str):
        self._log("warn", message)

    def _log_error(self, message: str):
        self._log("error", message)

    def _log_debug(self, message: str):
        self._log("debug", message)

    @property
    def is_connected(self) -> bool:
        """Check if client is connected to server."""
        return self._connected and self._websocket is not None

    @property
    def last_status(self) -> Optional[StatusMessage]:
        """Get the most recent status message."""
        return self._last_status

    @property
    def latest_result(self) -> Optional[PolicyResult]:
        """Get the most recent policy result."""
        return self._latest_policy_result

    @property
    def results_received_count(self) -> int:
        """Get the number of policy results received."""
        return self._results_received_count

    async def connect(self, config: SentinelConfig) -> bool:
        """
        Connect to Sentinel server and send configuration.

        Args:
            config: Configuration for this session

        Returns:
            True if connection successful, False otherwise
        """
        self._config = config
        self._should_reconnect = self.auto_reconnect

        try:
            self._log_info(f"Connecting to Sentinel server: {self.server_url}")

            # Match Pipecat reference: simple connect with max_size=None only
            self._websocket = await websockets.connect(
                self.server_url,
                max_size=None,
            )

            self._log_info("WebSocket connected, sending configuration...")

            # Send configuration as raw dict (matching Pipecat format exactly)
            # Note: Account must have Sentinel access enabled by Thymia
            config_dict = config.to_dict()
            await self._websocket.send(json.dumps(config_dict))
            self._log_info("Sentinel configuration sent")

            # Start receiving server events (before setting _connected, like Pipecat)
            self._receive_task = asyncio.create_task(self._receive_server_events())
            self._connected = True
            self._reconnect_delay = self.RECONNECT_DELAY_INITIAL

            return True

        except Exception as e:
            self._log_error(f"Failed to connect to Sentinel server: {e}")
            self._connected = False
            return False

    async def disconnect(self):
        """Disconnect from Sentinel server."""
        self._should_reconnect = False
        self._connected = False

        if self._receive_task:
            self._receive_task.cancel()
            try:
                await self._receive_task
            except asyncio.CancelledError:
                pass
            self._receive_task = None

        if self._websocket:
            try:
                await self._websocket.close()
            except Exception as e:
                self._log_warn(f"Error closing WebSocket: {e}")
            self._websocket = None

        self._log_info("Disconnected from Sentinel server")

    async def send_audio(self, pcm_data: bytes, track: str = "user") -> bool:
        """
        Send audio data to Sentinel server.

        Args:
            pcm_data: Raw PCM audio data (16-bit, mono, 16kHz)
            track: Audio track - "user" for user audio, "agent" for agent audio

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.is_connected:
            return False

        async with self._audio_send_lock:
            try:
                # Send header
                header = AudioHeader(
                    track=track,
                    bytes=len(pcm_data),
                )
                await self._websocket.send(json.dumps(header.to_dict()))

                # Send audio data
                await self._websocket.send(pcm_data)
                return True

            except Exception as e:
                self._log_error(f"Error sending {track} audio: {e}")
                return False

    async def send_transcript(
        self,
        speaker: str,
        text: str,
        is_final: bool = True,
        language: Optional[str] = None,
    ) -> bool:
        """
        Send transcript to Sentinel server.

        Args:
            speaker: Speaker identifier - "user" or "agent"
            text: Transcript text
            is_final: Whether this is a final transcript (vs interim)
            language: Optional language code (e.g., "en-GB")

        Returns:
            True if sent successfully, False otherwise
        """
        if not self.is_connected:
            return False

        if not text:
            return True  # Empty text, nothing to send

        try:
            transcript = TranscriptMessage(
                speaker=speaker,
                text=text,
                is_final=is_final,
                language=language or (self._config.language if self._config else None),
                timestamp=time.time(),
            )
            await self._websocket.send(json.dumps(transcript.to_dict()))
            self._log_debug(f"TRANSCRIPT [{speaker}]: {text[:100]}...")
            return True

        except Exception as e:
            self._log_error(f"Error sending {speaker} transcript: {e}")
            return False

    async def _receive_server_events(self):
        """Background task to receive and handle server events."""
        # Match Pipecat reference: simpler structure
        try:
            while self._websocket:
                message_json = await self._websocket.recv()
                message = json.loads(message_json)
                await self._handle_server_message(message)

        except ConnectionClosed:
            self._log_info("Server closed connection")

        except asyncio.CancelledError:
            self._log_debug("Receive task cancelled")
            raise

        except Exception as e:
            self._log_error(f"Error receiving server events: {e}")
            self._log_error(traceback.format_exc())

        finally:
            was_connected = self._connected
            self._connected = False

            if was_connected and self._should_reconnect and self._config:
                asyncio.create_task(self._reconnect())

    async def _handle_server_message(self, message: dict):
        """Handle a message received from the server."""
        event_type = message.get("type")
        self._log_debug(f"Received server message: type={event_type}")

        if event_type == "STATUS":
            status = StatusMessage.from_dict(message)
            self._last_status = status

            self._log_info(
                f"Buffer status: {status.buffer_duration:.1f}s buffered, "
                f"{status.speech_duration:.1f}s speech"
            )

            if self.on_status:
                await self._invoke_callback(self.on_status, status)

        elif event_type == "POLICY_RESULT":
            result = PolicyResult.from_dict(message)
            self._latest_policy_result = result
            self._results_received_count += 1

            self._log_policy_result(message)

            if self.on_policy_result:
                await self._invoke_callback(self.on_policy_result, result)

        elif event_type == "ERROR":
            error = ErrorMessage.from_dict(message)

            self._log_error(f"Server error [{error.error_code}]: {error.message}")
            if error.details:
                self._log_error(f"  Details: {error.details}")

            if self.on_error:
                await self._invoke_callback(self.on_error, error)

        elif event_type == "READY":
            # Note: Sentinel API doesn't actually send READY, but handle it just in case
            self._log_info("Server ready to receive audio")

        else:
            self._log_debug(f"Unknown server message type: {event_type}")

    async def _invoke_callback(self, callback, *args):
        """Invoke a callback, handling both sync and async callbacks."""
        try:
            if asyncio.iscoroutinefunction(callback):
                await callback(*args)
            else:
                callback(*args)
        except Exception as e:
            self._log_error(f"Error in callback: {e}")
            self._log_error(traceback.format_exc())

    def _log_policy_result(self, message: dict):
        """Log a policy result in a readable format."""
        self._log_debug("=" * 60)
        self._log_debug(f"POLICY_RESULT [{message.get('policy', 'unknown')}]")
        self._log_debug("=" * 60)

        self._log_debug(
            f"  turn: {message.get('triggered_at_turn')} | "
            f"ts: {message.get('timestamp')}"
        )

        result = message.get("result", {})
        result_type = result.get("type", "unknown")

        if result_type == "safety_analysis":
            classification = result.get("classification", {})
            self._log_debug(
                f"  level: {classification.get('level')} | "
                f"alert: {classification.get('alert')} | "
                f"confidence: {classification.get('confidence')}"
            )

            concerns = result.get("concerns", [])
            if concerns:
                self._log_debug(f"  concerns: {concerns}")

            actions = result.get("recommended_actions", {})
            if actions.get("for_agent"):
                self._log_debug(f"  for_agent: {actions['for_agent']}")

            # Log biomarkers if present
            biomarkers = result.get("biomarker_summary", {})
            if biomarkers:
                self._log_debug("  biomarkers:")
                for key in [
                    "distress",
                    "stress",
                    "burnout",
                    "fatigue",
                    "low_self_esteem",
                ]:
                    value = biomarkers.get(key)
                    if value is not None:
                        self._log_debug(f"    {key}: {value:.2%}")
                for key in ["depression_probability", "anxiety_probability"]:
                    value = biomarkers.get(key)
                    if value is not None:
                        self._log_debug(f"    {key}: {value:.2%}")

        elif result_type == "biomarker_passthrough":
            # Passthrough policy - just biomarkers
            biomarkers = result.get("biomarkers", {})
            if biomarkers:
                self._log_debug("  biomarkers:")
                for key, value in biomarkers.items():
                    if value is not None:
                        if isinstance(value, float):
                            self._log_debug(f"    {key}: {value:.2%}")
                        else:
                            self._log_debug(f"    {key}: {value}")

        else:
            # Generic result
            self._log_debug(f"  result_type: {result_type}")
            for key, value in result.items():
                if key != "type":
                    self._log_debug(f"    {key}: {value}")

        self._log_debug("=" * 60)

    async def _reconnect(self):
        """Attempt to reconnect to the server."""
        if not self._should_reconnect or not self._config:
            return

        self._log_info(f"Attempting reconnect in {self._reconnect_delay:.1f}s...")
        await asyncio.sleep(self._reconnect_delay)

        # Increase delay for next attempt (exponential backoff)
        self._reconnect_delay = min(
            self._reconnect_delay * self.RECONNECT_DELAY_MULTIPLIER,
            self.RECONNECT_DELAY_MAX,
        )

        # Attempt reconnect
        success = await self.connect(self._config)
        if not success and self._should_reconnect:
            # Schedule another reconnect attempt
            asyncio.create_task(self._reconnect())
