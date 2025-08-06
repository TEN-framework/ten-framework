import asyncio
import websockets
import datetime
import hashlib
import base64
import hmac
import os
import urllib.parse
import uuid
from urllib.parse import urlencode
import time
import ssl
from wsgiref.handlers import format_date_time
from datetime import datetime
from time import mktime
import json
from .const import TIMEOUT_CODE
from collections import OrderedDict

# 导入 websockets 相关的异常和状态
from websockets.exceptions import ConnectionClosed
from websockets.protocol import State

STATUS_FIRST_FRAME = 0  # First frame identifier
STATUS_CONTINUE_FRAME = 1  # Middle frame identifier
STATUS_LAST_FRAME = 2  # Last frame identifier


class XfyunWSRecognitionCallback:
    """WebSocket Speech Recognition Callback Interface"""

    async def on_open(self):
        """Called when connection is established"""
        pass

    async def on_result(self, message_data):
        """
        Recognition result callback
        :param message_data: Complete recognition result data
        """
        pass

    async def on_error(self, error_msg, error_code=None):
        """Error callback"""
        pass

    async def on_close(self):
        """Called when connection is closed"""
        pass


class XfyunWSRecognition:
    """Async WebSocket-based speech recognition class using new Xfyun ASR dialect API"""

    def __init__(self, app_id, access_key_id, access_key_secret, ten_env=None, config=None, callback=None):
        """
        Initialize WebSocket speech recognition with new API
        :param app_id: Application ID
        :param access_key_id: Access Key ID
        :param access_key_secret: Access Key Secret
        :param ten_env: Ten environment object for logging
        :param config: Configuration parameter dictionary, including the following optional parameters:
        :param callback: Callback function instance
        """
        self.app_id = app_id
        self.access_key_id = access_key_id
        self.access_key_secret = access_key_secret
        self.ten_env = ten_env

        # Set default configuration
        default_config = {
            "host": "office-api-ast-dx.iflyaisol.com",
            "lang": "autodialect",
            "audio_encode": "pcm",
            "samplerate": "16000",
            "multiFuncData": "false",
            "use_tts": "false",
            "nrtMode": "true"
        }

        # Merge user configuration and default configuration
        if config is None:
            config = {}
        self.config = {**default_config, **config}

        self.host = self.config["host"]
        self.callback = callback

        self.websocket = None
        self.is_started = False
        self.is_first_frame = True
        self._message_task = None

    def _log_debug(self, message):
        """Unified logging method, use ten_env.log_debug if available, otherwise use print"""
        if self.ten_env:
            self.ten_env.log_debug(message)
        else:
            print(message)

    def _get_params_string(self, params):
        """Convert parameters to URL parameter string"""
        result = []
        params = sorted(params.items(), key=lambda x: x[0])
        for key, value in params:
            encoded_value = urllib.parse.quote(value)
            result.append(f"{key}={encoded_value}")
        return "&".join(result)

    def _signature(self, access_key_secret, params):
        """
        Generate HMAC-SHA1 signature
        :param access_key_secret: Signature key
        :param params: Parameters to be signed
        :return: Base64 encoded signature
        """
        # 1. Filter parameters
        filtered_params = {
            k: v for k, v in params.items()
            if v is not None and v != "" and k != "signature"
        }

        # 2. Sort by parameter name ASCII code in ascending order
        sorted_params = sorted(filtered_params.items(), key=lambda x: x[0])

        # 3. Build string to be signed
        base_string = "&".join(
            f"{urllib.parse.quote(k)}={urllib.parse.quote(v)}"
            for k, v in sorted_params
        )

        # 4. Calculate HMAC-SHA1 signature
        digest = hmac.new(
            access_key_secret.encode("utf-8"),
            base_string.encode("utf-8"),
            hashlib.sha1
        ).digest()

        # 5. Base64 encoding
        return base64.b64encode(digest).decode("utf-8")

    def _get_access_url(self, extend_param, access_key_id, app_id, access_key_secret):
        """
        Generate access URL with signature
        :param extend_param: All request parameters (before URL encoding)
        :param access_key_id: Credential issued
        :param app_id: Business identifier issued
        :param access_key_secret: Secret key issued
        :return: URL parameter string
        """
        # Generate UTC+8 time (Beijing time), format: 2025-03-24T00:01:19+0800
        utc = datetime.now().astimezone().strftime("%Y-%m-%dT%H:%M:%S%z")
        # Ensure format is +0800 (not +08:00)
        utc = utc[:-2] + utc[-2:]

        extend_param["accessKeyId"] = access_key_id
        extend_param["appId"] = app_id
        extend_param["uuid"] = str(uuid.uuid4())
        extend_param["utc"] = utc
        signature_val = self._signature(access_key_secret, extend_param)
        extend_param["signature"] = signature_val
        return self._get_params_string(extend_param)

    def _create_url(self):
        """Generate WebSocket connection URL"""
        base_url = f'wss://{self.host}/ast/communicate/v1'

        # Build request parameters
        params = {}

        # Required parameters
        params["audio_encode"] = self.config.get("audio_encode", "pcm")
        params["samplerate"] = self.config.get("samplerate", "16000")
        params["lang"] = self.config.get("lang", "autodialect")
        params["codec"] = self.config.get("codec", "pcm")
        params["accent"] = self.config.get("accent", "mandarin")
        params["multiFuncData"] = self.config.get("multiFuncData", "false")
        params["use_tts"] = self.config.get("use_tts", "false")
        params["nrtMode"] = self.config.get("nrtMode", "true")

        # Optional parameters
        optional_params = [
            "multiFuncData", "use_tts", "nrtMode"
        ]

        list_params = []
        for param in optional_params:
            if param in self.config:
                list_params.append((param, str(self.config[param])))

        for param in params:
            if param in self.config:
                list_params.append((param, str(params[param])))

        params = OrderedDict(list_params)
        # Generate URL with signature
        url_params = self._get_access_url(params, self.access_key_id, self.app_id, self.access_key_secret)
        return f"{base_url}?{url_params}"

    async def _handle_message(self, message):
        """Handle WebSocket message"""
        try:
            message_data = json.loads(message)
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            self._log_debug(f"[{timestamp}] message: {message}")

            msg_type = message_data.get("msg_type")

            if msg_type == "action":
                # Handle action messages (started/end)
                data = message_data.get("data", {})
                action = data.get("action")

                if action == "started":
                    self._log_debug("ASR service started successfully")
                    if self.callback:
                        await self.callback.on_open()
                elif action == "end":
                    # Handle error or service closure
                    code = data.get("code")
                    error_message = data.get("message", "Service ended")
                    self._log_debug(f"ASR service ended: {error_message}, code: {code}")
                    if self.callback:
                        await self.callback.on_error(error_message, code)

            elif msg_type == "result":
                # Handle recognition results
                res_type = message_data.get("res_type")
                if res_type == "asr":
                    if self.callback:
                        await self.callback.on_result(message_data)

        except Exception as e:
            error_msg = f"Error processing message: {e}"
            self._log_debug(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {error_msg}")
            if self.callback:
                await self.callback.on_error(error_msg)

    async def _message_handler(self):
        """Handle incoming WebSocket messages"""
        try:
            if self.websocket:
                async for message in self.websocket:
                    await self._handle_message(message)
        except ConnectionClosed:
            self._log_debug("WebSocket connection closed")
        except Exception as e:
            error_msg = f"WebSocket message handler error: {e}"
            self._log_debug(f"### {error_msg} ###")
            if self.callback:
                await self.callback.on_error(error_msg)
        finally:
            self.is_started = False
            if self.callback:
                await self.callback.on_close()

    async def start(self, timeout=10):
        """
        Start speech recognition service
        :param timeout: Connection timeout in seconds, default 10 seconds
        """
        if self.is_started:
            self._log_debug("Recognition already started")
            return True

        try:
            ws_url = self._create_url()
            self._log_debug(f"Connecting to: {ws_url}")

            # Create SSL context that doesn't verify certificates (similar to original)
            ssl_context = ssl.create_default_context()
            ssl_context.check_hostname = False
            ssl_context.verify_mode = ssl.CERT_NONE

            # Connect to WebSocket with timeout
            self.websocket = await websockets.connect(
                ws_url,
                ssl=ssl_context,
                open_timeout=timeout
            )

            self._log_debug("### WebSocket opened ###")
            self.is_first_frame = True
            self.is_started = True

            # Start message handler task
            self._message_task = asyncio.create_task(self._message_handler())

            # The on_open callback will be triggered by the 'started' action message
            # from the server, not here, to maintain compatibility with the original behavior

            self._log_debug("Recognition started successfully")
            return True

        except asyncio.TimeoutError:
            error_msg = f"Connection timeout after {timeout} seconds"
            self._log_debug(f"Failed to start recognition: {error_msg}")
            if self.callback:
                await self.callback.on_error(error_msg, TIMEOUT_CODE)
            return False
        except Exception as e:
            error_msg = f"Failed to start recognition: {e}"
            self._log_debug(error_msg)
            if self.callback:
                await self.callback.on_error(error_msg)
            return False

    async def send_audio_frame(self, audio_data):
        """
        Send audio frame data
        :param audio_data: Audio data (bytes format)
        """
        if not self.is_started or not self.websocket:
            self._log_debug("Recognition not started")
            return

        try:
            # For the dialect API, we send raw binary audio data directly
            await self.websocket.send(audio_data)

        except ConnectionClosed:
            self._log_debug("WebSocket connection closed while sending audio frame")
            self.is_started = False
        except Exception as e:
            self._log_debug(f"Failed to send audio frame: {e}")
            if self.callback:
                await self.callback.on_error(f"Failed to send audio frame: {e}")

    async def stop(self):
        """
        Stop speech recognition
        """
        if not self.is_started or not self.websocket:
            self._log_debug("Recognition not started")
            return

        try:
            # Send end frame as text message
            end_message = json.dumps({"end": True})
            await self.websocket.send(end_message)
            self._log_debug("Stop signal sent")

        except ConnectionClosed:
            self._log_debug("WebSocket connection already closed")
        except Exception as e:
            self._log_debug(f"Failed to stop recognition: {e}")
            if self.callback:
                await self.callback.on_error(f"Failed to stop recognition: {e}")

    async def close(self):
        """Close WebSocket connection"""
        if not self.is_started:
            self._log_debug("Recognition not started")
            return

        if self.websocket:
            try:
                if self.websocket.state == State.OPEN:
                    await self.websocket.close()
            except Exception as e:
                self._log_debug(f"Error closing websocket: {e}")

        if self._message_task and not self._message_task.done():
            self._message_task.cancel()
            try:
                await self._message_task
            except asyncio.CancelledError:
                pass

        self.is_started = False
        self.is_first_frame = True
        self._log_debug("WebSocket connection closed")

    def is_connected(self) -> bool:
        """Check if WebSocket connection is established"""
        if self.websocket is None:
            return False

        # Check if websocket is still open by checking the state
        try:
            # For websockets library, we can check the state attribute
            if hasattr(self.websocket, 'state'):
                return self.is_started and self.websocket.state == State.OPEN
            # Fallback: just check if websocket exists and is_started is True
            else:
                return self.is_started
        except Exception:
            # If any error occurs, assume disconnected
            return False
