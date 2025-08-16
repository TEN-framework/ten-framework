# coding=utf-8

"""
Bytedance ASR WebSocket Client

Requires Python 3.9 or later for modern typing features.

Dependencies:
    websockets~=14.0
    pydantic
"""

import asyncio
import base64
import gzip
import hmac
import json
import logging
import uuid
from hashlib import sha256
from urllib.parse import urlparse
import websockets

from ten_runtime import AsyncTenEnv

PROTOCOL_VERSION = 0b0001
DEFAULT_HEADER_SIZE = 0b0001

PROTOCOL_VERSION_BITS = 4
HEADER_BITS = 4
MESSAGE_TYPE_BITS = 4
MESSAGE_TYPE_SPECIFIC_FLAGS_BITS = 4
MESSAGE_SERIALIZATION_BITS = 4
MESSAGE_COMPRESSION_BITS = 4
RESERVED_BITS = 8

# Message Type:
CLIENT_FULL_REQUEST = 0b0001
CLIENT_AUDIO_ONLY_REQUEST = 0b0010
SERVER_FULL_RESPONSE = 0b1001
SERVER_ACK = 0b1011
SERVER_ERROR_RESPONSE = 0b1111

# Message Type Specific Flags
NO_SEQUENCE = 0b0000  # no check sequence
POS_SEQUENCE = 0b0001
NEG_SEQUENCE = 0b0010
NEG_SEQUENCE_1 = 0b0011

# Message Serialization
NO_SERIALIZATION = 0b0000
JSON = 0b0001
THRIFT = 0b0011
CUSTOM_TYPE = 0b1111

# Message Compression
NO_COMPRESSION = 0b0000
GZIP = 0b0001
CUSTOM_COMPRESSION = 0b1111


def generate_header(
    version=PROTOCOL_VERSION,
    message_type=CLIENT_FULL_REQUEST,
    message_type_specific_flags=NO_SEQUENCE,
    serial_method=JSON,
    compression_type=GZIP,
    reserved_data=0x00,
    extension_header=bytes(),
):
    """
    protocol_version(4 bits), header_size(4 bits),
    message_type(4 bits), message_type_specific_flags(4 bits)
    serialization_method(4 bits) message_compression(4 bits)
    reserved (8bits) reserved field
    header_extensions extension header (size equals 8 * 4 * (header_size - 1))
    """
    header = bytearray()
    header_size = int(len(extension_header) / 4) + 1
    header.append((version << 4) | header_size)
    header.append((message_type << 4) | message_type_specific_flags)
    header.append((serial_method << 4) | compression_type)
    header.append(reserved_data)
    header.extend(extension_header)
    return header


def generate_full_default_header():
    return generate_header()


def generate_audio_default_header():
    return generate_header(message_type=CLIENT_AUDIO_ONLY_REQUEST)


def generate_last_audio_default_header():
    return generate_header(
        message_type=CLIENT_AUDIO_ONLY_REQUEST,
        message_type_specific_flags=NEG_SEQUENCE,
    )


def parse_response(res):
    """
    protocol_version(4 bits), header_size(4 bits),
    message_type(4 bits), message_type_specific_flags(4 bits)
    serialization_method(4 bits) message_compression(4 bits)
    reserved (8bits) reserved field
    header_extensions extension header (size equals 8 * 4 * (header_size - 1))
    payload similar to http request body
    """
    header_size = res[0] & 0x0F
    message_type = res[1] >> 4
    serialization_method = res[2] >> 4
    message_compression = res[2] & 0x0F
    payload = res[header_size * 4 :]
    result = {}
    payload_msg = None
    payload_size = 0
    if message_type == SERVER_FULL_RESPONSE:
        payload_size = int.from_bytes(payload[:4], "big", signed=True)
        payload_msg = payload[4:]
    elif message_type == SERVER_ACK:
        seq = int.from_bytes(payload[:4], "big", signed=True)
        result["seq"] = seq
        if len(payload) >= 8:
            payload_size = int.from_bytes(payload[4:8], "big", signed=False)
            payload_msg = payload[8:]
    elif message_type == SERVER_ERROR_RESPONSE:
        code = int.from_bytes(payload[:4], "big", signed=False)
        result["code"] = code
        payload_size = int.from_bytes(payload[4:8], "big", signed=False)
        payload_msg = payload[8:]
    if payload_msg is None:
        return result
    if message_compression == GZIP:
        payload_msg = gzip.decompress(payload_msg)
    if serialization_method == JSON:
        payload_msg = json.loads(str(payload_msg, "utf-8"))
    elif serialization_method != NO_SERIALIZATION:
        payload_msg = str(payload_msg, "utf-8")
    result["payload_msg"] = payload_msg
    result["payload_size"] = payload_size
    return result


class AsrWsClient:
    def __init__(self, ten_env: AsyncTenEnv, cluster, **kwargs):
        """
        :param config: config
        """
        self.cluster = cluster
        self.success_code = 1000  # success code, default is 1000
        # Optimized defaults to support fast finalize
        self.seg_duration = int(
            kwargs.get("seg_duration", 3000)
        )  # Reduce segment duration for faster results
        self.nbest = int(kwargs.get("nbest", 1))
        self.appid = kwargs.get("appid", "")
        self.token = kwargs.get("token", "")
        self.ws_url = kwargs.get(
            "ws_url", "wss://openspeech.bytedance.com/api/v2/asr"
        )
        self.uid = kwargs.get("uid", "streaming_asr_demo")
        self.workflow = kwargs.get(
            "workflow",
            "audio_in,resample,partition,vad,fe,decode,itn,nlu_punctuate",
        )
        self.show_language = kwargs.get("show_language", False)
        self.show_utterances = kwargs.get(
            "show_utterances", True
        )  # Ensure complete utterances information is returned
        self.result_type = kwargs.get(
            "result_type", "single"
        )  # Set appropriate result_type
        self.format = kwargs.get("format", "raw")
        self.rate = kwargs.get("sample_rate", 16000)
        self.language = kwargs.get("language", "zh-CN")
        self.bits = kwargs.get("bits", 16)
        self.channel = kwargs.get("channel", 1)
        self.codec = kwargs.get("codec", "raw")
        self.secret = kwargs.get("secret", "access_secret")
        self.auth_method = kwargs.get("auth_method", "token")
        self.mp3_seg_size = int(kwargs.get("mp3_seg_size", 10000))

        self.websocket = None
        self.handle_received_message = kwargs.get(
            "handle_received_message", self.default_handler
        )
        self.ten_env = ten_env

        # Add finalize-related state management
        self._finalize_requested = False

        # Test simulation: Check if we should simulate reconnectable errors
        self._test_error_simulation = None
        self._error_simulation_count = 0
        if self.appid == "test_reconnection_1003":
            self._test_error_simulation = 1003  # Simulate rate limit error
            self.ten_env.log_info(
                "=== TEST MODE: Will simulate 1003 (Rate Limit) errors ==="
            )
        elif self.token == "simulate_server_busy":
            self._test_error_simulation = 1005  # Simulate server busy error
            self.ten_env.log_info(
                "=== TEST MODE: Will simulate 1005 (Server Busy) errors ==="
            )
        self._finalize_completed = False
        self._finalize_event = asyncio.Event()

        # Finalize-related configuration
        self.send_empty_audio_on_finalize = kwargs.get(
            "send_empty_audio_on_finalize", True
        )

        # Add finalize completion callback
        self.on_finalize_complete = kwargs.get("on_finalize_complete", None)

        # Add error callback for handling non-1000 error codes
        self.on_error = kwargs.get("on_error", None)

    def default_handler(self, result):
        # Default handler if none is provided
        logging.warning("Received message but no handler is set: %s", result)

    async def receive_messages(self):
        while True:
            try:
                if not self.websocket:
                    self.ten_env.log_error(
                        "Websocket is None, cannot receive messages"
                    )
                    break
                res = await self.websocket.recv()
                result = parse_response(res)

                # Test simulation: Inject simulated errors for testing reconnection
                if (
                    self._test_error_simulation
                    and self._error_simulation_count < 3
                ):
                    self._error_simulation_count += 1
                    simulated_error_msg = f"Simulated ASR server error code: {self._test_error_simulation}"
                    self.ten_env.log_info(
                        f"=== SIMULATING ERROR {self._test_error_simulation} (attempt {self._error_simulation_count}/3) ==="
                    )
                    if self.on_error:
                        try:
                            await self.on_error(
                                self._test_error_simulation, simulated_error_msg
                            )
                        except Exception as callback_error:
                            self.ten_env.log_error(
                                f"Error in simulated error callback: {callback_error}"
                            )
                    continue  # Continue to trigger reconnection

                # Check result code, trigger error handling if not 1000
                result_code = result.get("code")
                if result_code is not None and result_code != self.success_code:
                    error_msg = f"ASR server returned error code: {result_code}"
                    self.ten_env.log_error(error_msg)
                    # Use error callback to handle non-1000 error codes
                    if self.on_error:
                        try:
                            await self.on_error(result_code, error_msg)
                        except Exception as callback_error:
                            self.ten_env.log_error(
                                f"Error in error callback: {callback_error}"
                            )
                    continue  # Continue listening, don't interrupt connection

                # Process received message
                await self.handle_received_message(
                    result["payload_msg"].get("result")
                )

                # Check if final result is received
                if self._finalize_requested and result["payload_msg"].get(
                    "result"
                ):
                    for item in result["payload_msg"]["result"]:
                        if "utterances" in item and item["utterances"]:
                            for utterance in item["utterances"]:
                                if utterance.get("definite", False):
                                    self._finalize_completed = True
                                    self._finalize_event.set()
                                    self.ten_env.log_info(
                                        "Received final ASR result"
                                    )

                                    # Call finalize completion callback
                                    if self.on_finalize_complete:
                                        try:
                                            await self.on_finalize_complete()
                                        except Exception as e:
                                            self.ten_env.log_error(
                                                f"Error in finalize complete callback: {e}"
                                            )

                                    return

            except websockets.ConnectionClosed as e:
                self.ten_env.log_info(f"WebSocket connection closed: {e}")
                # Trigger reconnection if connection closes unexpectedly
                if self.on_error:
                    try:
                        await self.on_error(
                            2001, f"WebSocket connection closed: {e}"
                        )
                    except Exception as callback_error:
                        self.ten_env.log_error(
                            f"Error in connection closed callback: {callback_error}"
                        )
                break
            except websockets.InvalidState as e:
                self.ten_env.log_error(f"WebSocket invalid state: {e}")
                if self.on_error:
                    try:
                        await self.on_error(
                            2002, f"WebSocket invalid state: {e}"
                        )
                    except Exception as callback_error:
                        self.ten_env.log_error(
                            f"Error in invalid state callback: {callback_error}"
                        )
                break
            except websockets.ProtocolError as e:
                self.ten_env.log_error(f"WebSocket protocol error: {e}")
                if self.on_error:
                    try:
                        await self.on_error(
                            2003, f"WebSocket protocol error: {e}"
                        )
                    except Exception as callback_error:
                        self.ten_env.log_error(
                            f"Error in protocol error callback: {callback_error}"
                        )
                break
            except asyncio.TimeoutError:
                self.ten_env.log_error("WebSocket receive timeout")
                if self.on_error:
                    try:
                        await self.on_error(1008, "WebSocket receive timeout")
                    except Exception as callback_error:
                        self.ten_env.log_error(
                            f"Error in timeout callback: {callback_error}"
                        )
                break
            except Exception as e:
                self.ten_env.log_error(
                    f"Unexpected error in receive_messages: {e}"
                )
                if self.on_error:
                    try:
                        await self.on_error(1007, f"Unexpected error: {e}")
                    except Exception as callback_error:
                        self.ten_env.log_error(
                            f"Error in unexpected error callback: {callback_error}"
                        )
                break

    async def finalize(self) -> None:
        """Send finalize signal to indicate end of audio input"""
        if not self.websocket:
            self.ten_env.log_warn("Websocket not connected, cannot finalize")
            return

        self._finalize_requested = True
        self.ten_env.log_info("Sending finalize signal to ASR server")

        if self.send_empty_audio_on_finalize:
            # Send an empty audio packet as end signal
            empty_audio = b""
            payload_bytes = gzip.compress(empty_audio)
            finalize_request = bytearray(generate_last_audio_default_header())
            finalize_request.extend(
                (len(payload_bytes)).to_bytes(4, "big")
            )  # payload size(4 bytes)
            finalize_request.extend(payload_bytes)  # payload

            try:
                await self.websocket.send(bytes(finalize_request))
                self.ten_env.log_info("Finalize signal sent successfully")
            except Exception as e:
                self.ten_env.log_error(f"Failed to send finalize signal: {e}")
        else:
            # Don't send empty audio packet, just set status
            self.ten_env.log_info(
                "Finalize requested without sending empty audio"
            )

    def is_finalized(self) -> bool:
        """Check if finalize has been completed"""
        return self._finalize_completed

    async def wait_for_finalize(self, timeout: float = 3.0) -> bool:
        """Wait for finalize completion with timeout"""
        try:
            await asyncio.wait_for(self._finalize_event.wait(), timeout=timeout)
            return True
        except asyncio.TimeoutError:
            self.ten_env.log_warn("Finalize timeout")
            return False

    async def start(self):
        reqid = str(uuid.uuid4())
        # Build full client request and serialize with compression
        request_params = self.construct_request(reqid)
        payload_bytes = str.encode(json.dumps(request_params))
        payload_bytes = gzip.compress(payload_bytes)
        full_client_request = bytearray(generate_full_default_header())
        full_client_request.extend(
            (len(payload_bytes)).to_bytes(4, "big")
        )  # payload size(4 bytes)
        full_client_request.extend(payload_bytes)  # payload
        header = None
        if self.auth_method == "token":
            header = self.token_auth()
        elif self.auth_method == "signature":
            header = self.signature_auth(full_client_request)
        self.websocket = await websockets.connect(
            self.ws_url, additional_headers=header, max_size=1000000000
        )
        # Send full client request
        await self.websocket.send(bytes(full_client_request))
        # Start receiving messages coroutine
        asyncio.create_task(self.receive_messages())

    async def finish(self) -> None:
        if self.websocket is not None:
            await self.websocket.close()
            self.websocket = None
            self.ten_env.log_info("Websocket connection closed.")
        else:
            self.ten_env.log_info("Websocket is not connected.")

    async def send(self, chunk: bytes):
        if not self.websocket:
            self.ten_env.log_error("Websocket is None, cannot send audio")
            return
        # if no compression, comment this line
        payload_bytes = gzip.compress(chunk)
        audio_only_request = bytearray(generate_audio_default_header())
        audio_only_request.extend(
            (len(payload_bytes)).to_bytes(4, "big")
        )  # payload size(4 bytes)
        audio_only_request.extend(payload_bytes)  # payload
        # Send audio-only client request
        await self.websocket.send(bytes(audio_only_request))

    def construct_request(self, reqid):
        req = {
            "app": {
                "appid": self.appid,
                "cluster": self.cluster,
                "token": self.token,
            },
            "user": {"uid": self.uid},
            "request": {
                "reqid": reqid,
                "nbest": self.nbest,
                "workflow": self.workflow,
                "show_language": self.show_language,
                "show_utterances": self.show_utterances,
                "result_type": self.result_type,
                "sequence": 1,
            },
            "audio": {
                "format": self.format,
                "rate": self.rate,
                "language": self.language,
                "bits": self.bits,
                "channel": self.channel,
                "codec": self.codec,
            },
        }
        return req

    def token_auth(self):
        self.ten_env.log_info(f"token_auth: {self.token}")
        return {"Authorization": "Bearer; {}".format(self.token)}

    def signature_auth(self, data):
        header_dicts = {
            "Custom": "auth_custom",
        }

        url_parse = urlparse(self.ws_url)
        input_str = "GET {} HTTP/1.1\n".format(url_parse.path)
        auth_headers = "Custom"
        for header in auth_headers.split(","):
            input_str += "{}\n".format(header_dicts[header])
        input_data = bytearray(input_str, "utf-8")
        input_data += data
        mac = base64.urlsafe_b64encode(
            hmac.new(
                self.secret.encode("utf-8"), input_data, digestmod=sha256
            ).digest()
        )
        header_dicts["Authorization"] = (
            'HMAC256; access_token="{}"; mac="{}"; h="{}"'.format(
                self.token, str(mac, "utf-8"), auth_headers
            )
        )
        return header_dicts
