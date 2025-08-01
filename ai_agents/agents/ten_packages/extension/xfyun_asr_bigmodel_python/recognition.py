import websocket
import datetime
import hashlib
import base64
import hmac
import os
from urllib.parse import urlencode
import time
import ssl
from wsgiref.handlers import format_date_time
from datetime import datetime
from time import mktime
import _thread as thread
import json
import threading
from .const import TIMEOUT_CODE

STATUS_FIRST_FRAME = 0  # First frame identifier
STATUS_CONTINUE_FRAME = 1  # Middle frame identifier
STATUS_LAST_FRAME = 2  # Last frame identifier



class XfyunWSRecognitionCallback:
    """WebSocket Speech Recognition Callback Interface"""

    def on_open(self):
        """Called when connection is established"""
        pass

    def on_result(self, message_data):
        """
        Recognition result callback
        :param message_data: Complete recognition result data
        """
        pass

    def on_error(self, error_msg, error_code=None):
        """Error callback"""
        pass

    def on_close(self):
        """Called when connection is closed"""
        pass


class XfyunWSRecognition:
    """WebSocket-based speech recognition class, interface design references recognition class in run.py"""

    def __init__(self, app_id, api_key, api_secret, ten_env=None, config=None, callback=None):
        """
        Initialize WebSocket speech recognition
        :param app_id: Application ID
        :param api_key: API key
        :param api_secret: API secret
        :param ten_env: Ten environment object for logging
        :param config: Configuration parameter dictionary, including the following optional parameters:
            - host: Server address, default "ist-api.xfyun.cn"
            - domain: Recognition domain, default "ist_ed_open"
            - language: Language, default "zh_cn"
            - accent: Accent, default "mandarin"
            - dwa: Whether to enable dynamic correction, default "wpgs"
            - request_id: Unique ID marking client request
            - eos: Endpoint detection silence time in milliseconds, default 99999999
            - pd: Domain personalization parameter (court/finance/medical/tech/sport/edu/isp/gov/game/ecom/mil/com/life/ent/culture/car)
            - res_id: Resource ID
            - vto: VAD hard cut control in ms, default 15000
            - punc: Punctuation control, default with punctuation, pass 0 to disable punctuation
            - nunum: Number normalization, 0 for no normalization, 1 for normalization, default 1
            - pptaw: Segmentation control, set how many words to cache before sending to segment prediction, ed default 450, small languages default 60
            - dyhotws: Whether to enable dynamic loading of hot words during conversation
            - personalization: Personalization parameters, supports PERSONAL/WFST/LM
            - seg_max: Post-processing segmentation function control maximum character count (0-500), ed default 140
            - seg_min: Post-processing segmentation function control minimum character count (0-50), ed default 0
            - seg_weight: Post-processing segmentation function control segment length weight (0-0.05), ed default 0.02
            - speex_size: Speex audio frame rate, only used with speex audio
            - spkdia: Real-time transcription role separation mode, 0 off/1 enable real-time turning point/2 enable real-time role separation, default 0
            - pgsnum: PGS truncation threshold, Chinese truncated by character count, English by word count, ed default 40, small languages default 800
            - vad_mdn: VAD far/near field switching, pass 2 to use near field, default far field
            - language_type: Language filter selection, 1 Chinese-English mode/2 Chinese mode/3 English mode/4 Pure Chinese mode/5 Chinese-English mixed mode, default 1
            - dhw: Session-level hot words, multiple hot words separated by English ','
            - dhw_mod: Session-level hot word mode, values [0,1,2], default 0
            - feature_list: Role separation voiceprint ID list
            - rsgid: Controls whether to return eseg_id field in result, 1 return, 0 not return
            - rlang: Cantonese traditional/simplified conversion switch, 1 simplified to traditional, 0 traditional to simplified
            - pgs_flash_freq: PGS refresh frequency, range 1-10, default 3
        :param callback: Callback function instance
        """
        self.app_id = app_id
        self.api_key = api_key
        self.api_secret = api_secret
        self.ten_env = ten_env

        # Set default configuration
        default_config = {
            "host": "ist-api.xfyun.cn",
            "domain": "ist_ed_open",
            "language": "zh_cn",
            "accent": "mandarin",
            "dwa": "wpgs"
        }

        # Merge user configuration and default configuration
        if config is None:
            config = {}
        self.config = {**default_config, **config}

        self.host = self.config["host"]
        self.callback = callback

        # Common parameters
        self.common_args = {"app_id": self.app_id}

        # Business parameters - extract all business-related parameters from config
        self.business_args = {}

        # Required business parameters
        required_business_params = ["domain", "language", "accent"]
        for param in required_business_params:
            if param in self.config:
                self.business_args[param] = self.config[param]

        # Optional business parameters
        optional_business_params = [
            "dwa", "request_id", "eos", "pd", "res_id", "vto", "punc", "nunum",
            "pptaw", "dyhotws", "personalization", "seg_max", "seg_min", "seg_weight",
            "speex_size", "spkdia", "pgsnum", "vad_mdn", "language_type", "dhw",
            "dhw_mod", "feature_list", "rsgid", "rlang", "pgs_flash_freq"
        ]
        for param in optional_business_params:
            if param in self.config:
                self.business_args[param] = self.config[param]

        self.ws = None
        self.is_started = False
        self.is_first_frame = True
        self.lock = threading.Lock()

    def _log_debug(self, message):
        """Unified logging method, use ten_env.log_debug if available, otherwise use print"""
        if self.ten_env:
            self.ten_env.log_debug(message)
        else:
            print(message)

    def _create_url(self):
        """Generate WebSocket connection URL"""
        url = f'wss://{self.host}/v2/ist'

        # Generate RFC1123 format timestamp
        now = datetime.now()
        date = format_date_time(mktime(now.timetuple()))

        # Concatenate string
        signature_origin = f"host: {self.host}\n"
        signature_origin += f"date: {date}\n"
        signature_origin += "GET /v2/ist HTTP/1.1"

        # Encrypt using hmac-sha256
        signature_sha = hmac.new(
            self.api_secret.encode('utf-8'),
            signature_origin.encode('utf-8'),
            digestmod=hashlib.sha256
        ).digest()
        signature_sha = base64.b64encode(signature_sha).decode(encoding='utf-8')

        authorization_origin = f'api_key="{self.api_key}", algorithm="hmac-sha256", headers="host date request-line", signature="{signature_sha}"'
        authorization = base64.b64encode(authorization_origin.encode('utf-8')).decode(encoding='utf-8')

        # Combine authentication parameters into dictionary
        v = {
            "authorization": authorization,
            "host": self.host,
            "date": date
        }
        url = url + '?' + urlencode(v)
        return url

    def _on_message(self, ws, message):
        """Handle WebSocket message"""
        try:
            message_data = json.loads(message)
            code = message_data.get("code")
            sid = message_data.get("sid")
            timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
            self._log_debug(f"[{timestamp}] message: {message}")
            self._log_debug(f"[{timestamp}] WebSocket callback triggered in thread: {thread.get_ident()}")

            if code != 0:
                error_msg = message_data.get("message")
                self._log_debug(f"[{timestamp}] sid: {sid} call error: {error_msg}, code: {code}")
                if self.callback:
                    self._log_debug(f"[{timestamp}] Calling callback.on_error")
                    self.callback.on_error(error_msg, code)
            else:
                data = message_data.get("data", {})

                if self.callback:
                    self._log_debug(f"[{timestamp}] Calling callback.on_result")
                    self.callback.on_result(message_data)

        except Exception as e:
            error_msg = f"Error processing message: {e}"
            self._log_debug(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] {error_msg}")
            if self.callback:
                self.callback.on_error(error_msg)

    def _on_error(self, ws, error):
        """Handle WebSocket error"""
        error_msg = f"WebSocket error: {error}"
        self._log_debug(f"### {error_msg} ###")
        if self.callback:
            self.callback.on_error(error_msg)

    def _on_close(self, ws, code, reason):
        """Handle WebSocket close"""
        self._log_debug("### WebSocket closed ###")
        self.is_started = False
        if self.callback:
            self.callback.on_close()

    def _on_open(self, ws):
        """Handle WebSocket connection establishment"""
        self._log_debug("### WebSocket opened ###")
        self.is_first_frame = True
        self.connection_established = True  # Mark connection as established
        if self.callback:
            self.callback.on_open()

    def start(self, timeout=10):
        """
        Start speech recognition service
        Similar to recognition.start() in run.py
        :param timeout: Connection timeout in seconds, default 10 seconds
        """
        if self.is_started:
            self._log_debug("Recognition already started")
            return True

        try:
            websocket.enableTrace(False)
            ws_url = self._create_url()
            self.ws = websocket.WebSocketApp(
                ws_url,
                on_message=self._on_message,
                on_error=self._on_error,
                on_close=self._on_close
            )
            self.ws.on_open = self._on_open

            # Add connection status flag
            self.connection_established = False
            self.connection_error = None

            # Run WebSocket in new thread
            def run_ws():
                try:
                    if self.ws is not None:
                        self.ws.run_forever(sslopt={"cert_reqs": ssl.CERT_NONE})
                except Exception as e:
                    self.connection_error = str(e)

            self.ws_thread = threading.Thread(target=run_ws)
            self.ws_thread.daemon = True
            self.ws_thread.start()

            # Wait for connection to establish, with timeout
            start_time = time.time()
            while not self.connection_established and not self.connection_error:
                if time.time() - start_time > timeout:
                    error_msg = f"Connection timeout after {timeout} seconds"
                    self._log_debug(f"Failed to start recognition: {error_msg}")
                    if self.callback:
                        self.callback.on_error(error_msg, TIMEOUT_CODE)
                    if self.ws is not None:
                        self.ws.close()
                    return False
                time.sleep(0.1)

            if self.connection_error:
                error_msg = f"Connection failed: {self.connection_error}"
                self._log_debug(f"Failed to start recognition: {error_msg}")
                if self.callback:
                    self.callback.on_error(error_msg)
                return False

            self.is_started = True
            self._log_debug("Recognition started successfully")
            return True

        except Exception as e:
            error_msg = f"Failed to start recognition: {e}"
            self._log_debug(error_msg)
            if self.callback:
                self.callback.on_error(error_msg)
            return False

    def send_audio_frame(self, audio_data):
        """
        Send audio frame data
        Similar to recognition.send_audio_frame(audio_data) in run.py
        This method is equivalent to the original start(data), handling first frame and middle frame data
        :param audio_data: Audio data (bytes format)
        """
        if not self.is_started or not self.ws:
            self._log_debug("Recognition not started")
            return

        try:
            if self.is_first_frame:
                # First frame data, needs to include business parameters
                d = {
                    "common": self.common_args,
                    "business": self.business_args,
                    "data": {
                        "status": STATUS_FIRST_FRAME,
                        "format": f"audio/L16;rate={self.config.get('sample_rate', 16000)}",
                        "audio": str(base64.b64encode(audio_data), 'utf-8'),
                        "encoding": "raw"
                    }
                }
                self.is_first_frame = False
            else:
                # Middle frame data
                d = {
                    "data": {
                        "status": STATUS_CONTINUE_FRAME,
                        "format": f"audio/L16;rate={self.config.get('sample_rate', 16000)}",
                        "audio": str(base64.b64encode(audio_data), 'utf-8'),
                        "encoding": "raw"
                    }
                }

            self.ws.send(json.dumps(d))

        except Exception as e:
            self._log_debug(f"Failed to send audio frame: {e}")
            if self.callback:
                self.callback.on_error(f"Failed to send audio frame: {e}")

    def stop(self):
        """
        Stop speech recognition
        Similar to recognition.stop() in run.py
        """
        if not self.is_started or not self.ws:
            self._log_debug("Recognition not started")
            return

        try:
            # Send end identifier
            d = {
                "data": {
                    "status": STATUS_LAST_FRAME,
                    "format": f"audio/L16;rate={self.config.get('sample_rate', 16000)}",
                    "audio": "",
                    "encoding": "raw"
                }
            }
            self.ws.send(json.dumps(d))
            self._log_debug("Stop signal sent")

        except Exception as e:
            self._log_debug(f"Failed to stop recognition: {e}")
            if self.callback:
                self.callback.on_error(f"Failed to stop recognition: {e}")

    def close(self):
        """Close WebSocket connection"""
        if self.ws:
            self.ws.close()
            self.is_started = False
            self.is_first_frame = True
            self._log_debug("WebSocket connection closed")

    def is_connected(self) -> bool:
        """Check if WebSocket connection is established"""
        if self.ws is None:
            return False
        if not self.ws.sock:
            return False
        return self.is_started
