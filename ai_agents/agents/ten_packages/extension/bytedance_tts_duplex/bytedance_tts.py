import asyncio
import json
from typing import AsyncGenerator, Tuple
import uuid

import time
import fastrand
from pydantic import BaseModel
import websockets
from websockets.legacy.client import WebSocketClientProtocol
from websockets.protocol import State

from ten_ai_base.message import (
    ModuleErrorVendorInfo,
    ModuleVendorException,
)
from .config import BytedanceTTSDuplexConfig
from ten_runtime import AsyncTenEnv

# https://www.volcengine.com/docs/6561/1329505#%E7%A4%BA%E4%BE%8Bsamples


PROTOCOL_VERSION = 0b0001
DEFAULT_HEADER_SIZE = 0b0001

# Message Type:
FULL_CLIENT_REQUEST = 0b0001
AUDIO_ONLY_RESPONSE = 0b1011
FULL_SERVER_RESPONSE = 0b1001
ERROR_INFORMATION = 0b1111

# Message Type Specific Flags
MsgTypeFlagNoSeq = 0b0000  # Non-terminal packet with no sequence
MsgTypeFlagPositiveSeq = 0b1  # Non-terminal packet with sequence > 0
MsgTypeFlagLastNoSeq = 0b10  # last packet with no sequence
MsgTypeFlagNegativeSeq = 0b11  # Payload contains event number (int32)
MsgTypeFlagWithEvent = 0b100
# Message Serialization
NO_SERIALIZATION = 0b0000
JSON = 0b0001
# Message Compression
COMPRESSION_NO = 0b0000
COMPRESSION_GZIP = 0b0001

EVENT_NONE = 0
EVENT_Start_Connection = 1

EVENT_FinishConnection = 2

EVENT_ConnectionStarted = 50  # connection successfully started

EVENT_ConnectionFailed = 51  # connection failed

EVENT_ConnectionFinished = 52  # connection finished

# start session event
EVENT_StartSession = 100

EVENT_FinishSession = 102
# 下行Session事件
EVENT_SessionStarted = 150
EVENT_SessionFinished = 152

EVENT_SessionFailed = 153

# client request
EVENT_TaskRequest = 200

# server response
EVENT_TTSSentenceStart = 350

EVENT_TTSSentenceEnd = 351

EVENT_TTSResponse = 352

# TODO all received
# TODO all sent
# TODO key points


class Header:
    def __init__(
        self,
        protocol_version=PROTOCOL_VERSION,
        header_size=DEFAULT_HEADER_SIZE,
        message_type: int = 0,
        message_type_specific_flags: int = 0,
        serial_method: int = NO_SERIALIZATION,
        compression_type: int = COMPRESSION_NO,
        reserved_data=0,
    ):
        self.header_size = header_size
        self.protocol_version = protocol_version
        self.message_type = message_type
        self.message_type_specific_flags = message_type_specific_flags
        self.serial_method = serial_method
        self.compression_type = compression_type
        self.reserved_data = reserved_data

    def as_bytes(self) -> bytes:
        return bytes(
            [
                (self.protocol_version << 4) | self.header_size,
                (self.message_type << 4) | self.message_type_specific_flags,
                (self.serial_method << 4) | self.compression_type,
                self.reserved_data,
            ]
        )


class Optional:
    def __init__(
        self,
        event: int = EVENT_NONE,
        sessionId: str = None,
        sequence: int = None,
    ):
        self.event = event
        self.sessionId = sessionId
        self.errorCode: int = 0
        self.connectionId: str | None = None
        self.response_meta_json: str | None = None
        self.sequence = sequence

    # to byte sequence
    def as_bytes(self) -> bytes:
        option_bytes = bytearray()
        if self.event != EVENT_NONE:
            option_bytes.extend(self.event.to_bytes(4, "big", signed=False))
        if self.sessionId is not None:
            session_id_bytes = str.encode(self.sessionId)
            size = len(session_id_bytes).to_bytes(4, "big", signed=False)
            option_bytes.extend(size)
            option_bytes.extend(session_id_bytes)
        if self.sequence is not None:
            option_bytes.extend(self.sequence.to_bytes(4, "big", signed=False))
        return option_bytes


class Response:
    def __init__(self, header: Header, optional: Optional):
        self.optional = optional
        self.header = header
        self.payload: bytes | None = None
        self.payload_json: str | None = None


class ServerResponse(BaseModel):
    event: int = EVENT_NONE
    sessionId: str | None = None
    response_meta_json: str | None = None
    payload: bytes | None = None
    payload_json: str | None = None
    header: dict | None = None
    optional: dict | None = None


def parser_response(res) -> Response:
    """Parse the response from the server."""
    if isinstance(res, str):
        raise RuntimeError(res)
    response = Response(Header(), Optional())

    # header
    header = response.header
    num = 0b00001111
    header.protocol_version = res[0] >> 4 & num
    header.header_size = res[0] & 0x0F
    header.message_type = (res[1] >> 4) & num
    header.message_type_specific_flags = res[1] & 0x0F
    header.serial_method = res[2] >> num
    header.compression_type = res[2] & 0x0F
    header.reserved_data = res[3]

    offset = 4
    optional = response.optional
    if header.message_type == FULL_SERVER_RESPONSE or AUDIO_ONLY_RESPONSE:
        # read event
        if header.message_type_specific_flags == MsgTypeFlagWithEvent:
            optional.event = int.from_bytes(
                res[offset : offset + 4], "big", signed=False
            )
            offset += 4
            if optional.event == EVENT_NONE:
                return response
            # read connectionId
            elif optional.event == EVENT_ConnectionStarted:
                optional.connectionId, offset = read_res_content(res, offset)
            elif optional.event == EVENT_ConnectionFailed:
                optional.response_meta_json, offset = read_res_content(
                    res, offset
                )
            elif (
                optional.event == EVENT_SessionStarted
                or optional.event == EVENT_SessionFailed
                or optional.event == EVENT_SessionFinished
            ):
                optional.sessionId, offset = read_res_content(res, offset)
                optional.response_meta_json, offset = read_res_content(
                    res, offset
                )
            elif optional.event == EVENT_TTSResponse:
                optional.sessionId, offset = read_res_content(res, offset)
                response.payload, offset = read_res_payload(res, offset)
            elif (
                optional.event == EVENT_TTSSentenceEnd
                or optional.event == EVENT_TTSSentenceStart
            ):
                optional.sessionId, offset = read_res_content(res, offset)
                response.payload_json, offset = read_res_content(res, offset)

    elif header.message_type == ERROR_INFORMATION:
        optional.errorCode = int.from_bytes(
            res[offset : offset + 4], "big", signed=False
        )
        offset += 4
        response.payload, offset = read_res_payload(res, offset)
    return response


def read_res_content(res: bytes, offset: int):
    """read content from response bytes"""
    content_size = int.from_bytes(res[offset : offset + 4], "big", signed=False)
    offset += 4
    content = str(res[offset : offset + content_size], encoding="utf8")
    offset += content_size
    return content, offset


def read_res_payload(res: bytes, offset: int):
    """read payload from response bytes"""
    payload_size = int.from_bytes(res[offset : offset + 4], "big", signed=False)
    offset += 4
    payload = res[offset : offset + payload_size]
    offset += payload_size
    return payload, offset


class BytedanceV3Client:
    def __init__(
        self,
        config: BytedanceTTSDuplexConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
        response_msgs: asyncio.Queue[Tuple[int, bytes]],
    ):
        self.config = config
        self.app_id = config.appid
        self.token = config.token
        self.speaker = config.voice_type
        self.session_id = uuid.uuid4().hex
        self.ws: WebSocketClientProtocol = None
        self.stop_event = asyncio.Event()
        self.ten_env: AsyncTenEnv = ten_env
        self.vendor = vendor
        self.response_msgs: asyncio.Queue[Tuple[int, bytes]] | None = (
            response_msgs
        )

    def gen_log_id(self) -> str:
        ts = int(time.time() * 1000)
        r = fastrand.pcg32bounded(1 << 24) + (1 << 20)
        local_ip = "00000000000000000000000000000000"
        return f"02{ts}{local_ip}{r:08x}"

    def get_headers(self):
        return {
            "X-Api-App-Key": self.app_id,
            "X-Api-Access-Key": self.token,
            "X-Api-Resource-Id": "volc.service_type.10029",
            "X-Api-Connect-Id": str(uuid.uuid4()),
            "X-Tt-Logid": self.gen_log_id(),
        }

    def get_payload_bytes(
        self,
        uid="1234",
        event=EVENT_NONE,
        text="",
        speaker="",
    ):
        """Generate payload bytes for the request."""
        json_params = {
            "user": {"uid": uid},
            "event": event,
            "namespace": "BidirectionalTTS",
            "req_params": {
                "text": text,
                "speaker": speaker,
                "audio_params": self.config.params["audio_params"],
                "additions": (
                    self.config.params["additions"]
                    if "additions" in self.config.params
                    else None
                ),
            },
        }
        json_str = json.dumps(json_params)
        self.ten_env.log_info(f"Payload JSON: {json_str}")
        return str.encode(json_str)

    async def connect(self):
        url = self.config.api_url
        self.ten_env.log_debug(
            f"Connecting to {url} with headers: {json.dumps(self.get_headers(), indent=2)}"
        )
        self.ws = await websockets.connect(
            url,
            additional_headers=self.get_headers(),
            max_size=100_000_000,
            compression=None,
        )

    async def send_event(
        self,
        header: bytes,
        optional: bytes | None = None,
        payload: bytes = None,
    ):
        if self.ws is not None:
            if self.ws.state != State.OPEN:
                self.ten_env.log_warn(
                    "WebSocket is not open, cannot send event"
                )
            else:
                full_client_request = bytearray(header)
                if optional is not None:
                    full_client_request.extend(optional)
                if payload is not None:
                    payload_size = len(payload).to_bytes(4, "big", signed=False)
                    full_client_request.extend(payload_size)
                    full_client_request.extend(payload)
                await self.ws.send(bytes(full_client_request))

    async def start_connection(self):
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(event=EVENT_Start_Connection).as_bytes()
        payload = b"{}"
        await self.send_event(header, optional, payload)
        res = self.parse_server_message(await self.ws.recv())
        self._print_response(res, "start_connection")
        if res.event != EVENT_ConnectionStarted:
            raise ModuleVendorException(
                ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code=str(res.event),
                    message=f"Start connection failed with event: {res.event}",
                )
            )

    async def start_session(self):
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_StartSession, sessionId=self.session_id
        ).as_bytes()
        payload = self.get_payload_bytes(
            event=EVENT_StartSession, speaker=self.speaker
        )
        await self.send_event(header, optional, payload)
        res = self.parse_server_message(await self.ws.recv())
        self._print_response(res, "start_session")
        if res.event != EVENT_SessionStarted:
            raise ModuleVendorException(
                ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code=str(res.event),
                    message=f"Start session failed with event: {res.event}",
                )
            )

        asyncio.create_task(self.recv_loop())

    async def send_text(self, text: str):
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_TaskRequest, sessionId=self.session_id
        ).as_bytes()
        payload = self.get_payload_bytes(
            event=EVENT_TaskRequest, text=text, speaker=self.speaker
        )
        await self.send_event(header, optional, payload)

    async def finish_session(self):
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_FinishSession, sessionId=self.session_id
        ).as_bytes()
        await self.send_event(header, optional, b"{}")

    async def finish_connection(self):
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(event=EVENT_FinishConnection).as_bytes()
        await self.send_event(header, optional, b"{}")

    async def listen(self) -> AsyncGenerator[ServerResponse, None]:
        assert self.ws is not None
        try:
            async for msg in self.ws:
                yield self.handle_server_message(msg)
        except asyncio.CancelledError:
            self.ten_env.log_info("Receive messages task cancelled")

    async def recv_loop(self):
        async for message in self.listen():
            self._print_response(message, "recv_loop")
            if message.event != EVENT_TTSResponse:
                self.ten_env.log_info(f"KEYPOINT Received message: {message}")
            if message.event == EVENT_TTSResponse:
                if message.payload and self.response_msgs is not None:
                    await self.response_msgs.put(
                        (message.event, message.payload)
                    )
                else:
                    self.ten_env.log_error(
                        "Received empty payload for TTS response"
                    )
            elif message.event == EVENT_TTSSentenceEnd:
                if self.response_msgs is not None:
                    await self.response_msgs.put((message.event, None))
            elif message.event == EVENT_SessionFinished:
                if self.response_msgs is not None:
                    await self.response_msgs.put((message.event, None))
            elif message.event in [
                EVENT_ConnectionFinished,
                EVENT_SessionFailed,
            ]:
                continue
            else:
                continue

    def handle_server_message(self, message: str) -> ServerResponse:
        try:
            return self.parse_server_message(message)
        except Exception as e:
            self.ten_env.log_info(f"Error handling message {e}")

    def parse_server_message(self, res) -> ServerResponse:
        try:
            response = parser_response(res)
            if (
                response.header.message_type == FULL_SERVER_RESPONSE
                or response.header.message_type == AUDIO_ONLY_RESPONSE
            ):
                return ServerResponse(
                    event=response.optional.event,
                    sessionId=response.optional.sessionId,
                    response_meta_json=response.optional.response_meta_json,
                    payload=response.payload,
                    payload_json=response.payload_json,
                    header=response.header.__dict__,
                    optional=response.optional.__dict__,
                )
            elif response.header.message_type == ERROR_INFORMATION:
                raise ModuleVendorException(
                    ModuleErrorVendorInfo(
                        vendor=self.vendor,
                        code=response.optional.event,
                        message=response.payload_json or "Unknown error",
                    )
                )
            else:
                raise RuntimeError(
                    f"unknown message type: {response.header.message_type}"
                )
        except json.JSONDecodeError as e:
            self.ten_env.log_error(f"Failed to parse server message: {e}")
            raise RuntimeError(f"Failed to parse server message: {e}") from e

    def _print_response(self, res: ServerResponse, tag: str):
        self.ten_env.log_debug(f"[{tag}] Header: {res.header}")
        self.ten_env.log_debug(f"[{tag}] Optional: {res.optional}")
        self.ten_env.log_debug(
            f"[{tag}] Payload Len: {len(res.payload) if res.payload else 0}"
        )
        self.ten_env.log_debug(f"[{tag}] Payload JSON: {res.payload_json}")

    async def close(self):
        # Close the websocket connection if it exists
        if self.ws:
            await self.ws.close()
            self.ws = None
        self.response_msgs = None
