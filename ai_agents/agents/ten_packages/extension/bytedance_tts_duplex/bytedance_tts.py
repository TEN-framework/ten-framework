import asyncio
import json
import time
import uuid
from collections.abc import Awaitable, Callable

import fastrand
import websockets
from pydantic import BaseModel
from websockets.legacy.client import WebSocketClientProtocol
from websockets.protocol import State

from ten_ai_base.const import LOG_CATEGORY_TRANSCRIPTS, LOG_CATEGORY_VENDOR
from ten_ai_base.message import ModuleError, ModuleErrorCode, ModuleErrorVendorInfo
from ten_runtime import AsyncTenEnv

from .config import BytedanceTTSDuplexConfig
import contextlib

# https://www.volcengine.com/docs/6561/1329505#%E7%A4%BA%E4%BE%8Bsamples

# Connection-related constants
MAX_RETRY_TIMES_FOR_TRANSPORT = 5
ERR_WS_CONNECTION = 0
NORMAL_WEBSOCKET_CLOSE_CODES = {1000, 1001}


def _module_code_for_connection_closed(e: websockets.ConnectionClosed) -> int:
    if e.code in NORMAL_WEBSOCKET_CLOSE_CODES:
        return 0
    return int(ModuleErrorCode.NON_FATAL_ERROR)


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
# Downstream Session events
EVENT_SessionStarted = 150
EVENT_SessionFinished = 152

EVENT_SessionFailed = 153

# client request
EVENT_TaskRequest = 200

# server response
EVENT_TTSSentenceStart = 350

EVENT_TTSSentenceEnd = 351

EVENT_TTSResponse = 352

EVENT_TTSSubtitle = 364

# Internal event used to forward protocol error frames to the extension.
EVENT_TTSError = -1

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
            ],
        )


class Optional:
    def __init__(
        self,
        event: int = EVENT_NONE,
        sessionId: str | None = None,
        sequence: int | None = None,
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
        return bytes(option_bytes)


class Response:
    def __init__(self, header: Header, optional: Optional):
        self.optional = optional
        self.header = header
        self.payload: bytes | None = None
        self.payload_json: str | None = None


class ServerResponse(BaseModel):
    received_at: float | None = None
    event: int = EVENT_NONE
    sessionId: str | None = None
    response_meta_json: str | None = None
    payload: bytes | None = None
    payload_json: str | None = None
    header: dict | None = None
    optional: dict | None = None
    error_code: int = 0
    error_message: str | None = None

    def __str__(self):
        return f"ServerResponse(event={self.event}, sessionId={self.sessionId}, response_meta_json={self.response_meta_json}, payload={self.payload}, payload_json={self.payload_json}, header={self.header}, optional={self.optional})"


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
    if header.message_type in (FULL_SERVER_RESPONSE, AUDIO_ONLY_RESPONSE):
        # read event
        if header.message_type_specific_flags == MsgTypeFlagWithEvent:
            optional.event = int.from_bytes(
                res[offset : offset + 4],
                "big",
                signed=False,
            )
            offset += 4
            if optional.event == EVENT_NONE:
                return response
            # read connectionId
            if optional.event == EVENT_ConnectionStarted:
                optional.connectionId, offset = read_res_content(res, offset)
            elif optional.event == EVENT_ConnectionFailed:
                optional.response_meta_json, offset = read_res_content(res, offset)
            elif optional.event in (
                EVENT_SessionStarted,
                EVENT_SessionFailed,
                EVENT_SessionFinished,
            ):
                optional.sessionId, offset = read_res_content(res, offset)
                optional.response_meta_json, offset = read_res_content(res, offset)
            elif optional.event == EVENT_TTSResponse:
                optional.sessionId, offset = read_res_content(res, offset)
                response.payload, offset = read_res_payload(res, offset)
            elif optional.event in (
                EVENT_TTSSentenceEnd,
                EVENT_TTSSentenceStart,
                EVENT_TTSSubtitle,
            ):
                optional.sessionId, offset = read_res_content(res, offset)
                response.payload_json, offset = read_res_content(res, offset)

    elif header.message_type == ERROR_INFORMATION:
        optional.errorCode = int.from_bytes(
            res[offset : offset + 4],
            "big",
            signed=False,
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


class BytedanceV3Synthesizer:
    def __init__(
        self,
        config: BytedanceTTSDuplexConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
        response_msgs: asyncio.Queue[tuple[int, bytes | dict | None]],
        on_error: Callable[[ModuleError], Awaitable[None]] | None = None,
        on_usage_characters: Callable[[int], None] | None = None,
        on_fatal_failure: Callable[[ModuleError], Awaitable[None]] | None = None,
        on_connection_connecting: Callable[[], Awaitable[None]] | None = None,
        on_connection_connected: Callable[[], Awaitable[None]] | None = None,
        on_connection_disconnected: Callable[..., Awaitable[None]] | None = None,
        on_text_send: Callable[[str, float], None] | None = None,
        on_audio_receive: Callable[[str, float], None] | None = None,
    ):
        self.config = config
        self.app_id = config.app_id
        self.api_key = config.api_key
        self.resource_id = config.resource_id
        self.token = config.token
        self.speaker = config.speaker
        self.model = config.model
        self.session_id = uuid.uuid4().hex
        self.ws: WebSocketClientProtocol | None = None
        self.stop_event = asyncio.Event()
        self.ten_env: AsyncTenEnv = ten_env
        self.vendor = vendor
        self.response_msgs: asyncio.Queue[tuple[int, bytes | dict | None]] | None = (
            response_msgs
        )
        self.on_error = on_error
        self.on_usage_characters = on_usage_characters
        self.on_fatal_failure = on_fatal_failure
        self.on_connection_connecting = on_connection_connecting
        self.on_connection_connected = on_connection_connected
        self.on_connection_disconnected = on_connection_disconnected
        self.on_text_send = on_text_send
        self.on_audio_receive = on_audio_receive

        # Connection management related
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        self.channel_tasks = []
        self._session_started = False
        self._connection_id: str | None = None
        self._connection_started_ms: int | None = None
        self._connect_id: str | None = None
        self._tt_logid: str | None = None

        # Queue for pending text to be sent
        self.text_queue = asyncio.Queue()

        # Mechanism for waiting for specific events
        self._connection_event = asyncio.Event()
        self._session_event = asyncio.Event()
        self._connection_success = False
        self._session_success = False
        self._receive_ready_event = asyncio.Event()

        # Start websocket connection monitoring
        self.websocket_task = asyncio.create_task(self._process_websocket())

    def gen_log_id(self) -> str:
        ts = int(time.time() * 1000)
        r = fastrand.pcg32bounded(1 << 24) + (1 << 20)
        local_ip = "00000000000000000000000000000000"
        return f"02{ts}{local_ip}{r:08x}"

    def get_headers(self):
        explicit_headers = self.config.headers or {}
        explicit_app_auth = explicit_headers.get(
            "X-Api-App-Key",
        ) and explicit_headers.get("X-Api-Access-Key")
        explicit_api_key_auth = explicit_headers.get("X-Api-Key")

        self._connect_id = str(uuid.uuid4())
        self._tt_logid = self.gen_log_id()
        headers = {
            "X-Api-Resource-Id": self.resource_id,
            "X-Api-Connect-Id": self._connect_id,
            "X-Tt-Logid": self._tt_logid,
            "X-Control-Require-Usage-Tokens-Return": "*",
        }

        if explicit_app_auth or explicit_api_key_auth:
            headers.update(explicit_headers)
            self._connect_id = headers.get("X-Api-Connect-Id")
            self._tt_logid = headers.get("X-Tt-Logid")
            return headers

        if self.app_id:
            headers.update(
                {
                    "X-Api-App-Key": self.app_id,
                    "X-Api-Access-Key": self.token,
                },
            )
        else:
            headers["X-Api-Key"] = self.api_key

        if explicit_headers:
            headers.update(explicit_headers)

        self._connect_id = headers.get("X-Api-Connect-Id")
        self._tt_logid = headers.get("X-Tt-Logid")

        return headers

    def get_payload_bytes(
        self,
        uid="1234",
        event=EVENT_NONE,
        text="",
        speaker="",
        model="",
    ):
        """Generate payload bytes for the request."""
        json_params = {
            "user": {"uid": uid},
            "event": event,
            "namespace": "BidirectionalTTS",
            "req_params": {
                "text": text,
                "speaker": speaker,
                "model": model,
                "audio_params": self.config.params["audio_params"],
                "additions": (
                    self.config.params["additions"]
                    if "additions" in self.config.params
                    else None
                ),
            },
        }
        json_str = json.dumps(json_params)
        self.ten_env.log_info(
            f"Payload JSON: text_length={len(text)}",
        )
        self.ten_env.log_debug(
            f"Payload JSON details: {json_str}",
            category=LOG_CATEGORY_TRANSCRIPTS,
        )
        return str.encode(json_str)

    def _process_ws_exception(self, exp) -> None | Exception:
        """Handle websocket connection exceptions and decide whether to reconnect"""
        self.ten_env.log_warn(f"Websocket internal error during connecting: {exp}.")
        self.ten_env.log_warn(
            "vendor_status: websocket_connect_error "
            f"ts_ms={int(time.time() * 1000)} error={exp!s}",
        )
        self._connect_exp_cnt += 1
        if self._connect_exp_cnt > MAX_RETRY_TIMES_FOR_TRANSPORT:
            self.ten_env.log_error(
                f"Max retries ({MAX_RETRY_TIMES_FOR_TRANSPORT}) exceeded: {exp!s}",
            )
            return exp
        return None  # Return None to continue reconnection

    async def _process_websocket(self) -> None:
        """Main websocket connection monitoring and reconnection logic"""
        terminal_error: ModuleError | None = None

        try:
            self.ten_env.log_info("Starting websocket connection process")
            if self.on_connection_connecting:
                await self.on_connection_connecting()
            # Use websockets.connect's automatic reconnection mechanism
            async for ws in websockets.connect(
                uri=self.config.api_url,
                additional_headers=self.get_headers(),
                max_size=100_000_000,
                compression=None,
                process_exception=self._process_ws_exception,
            ):
                self.ws = ws
                self._connection_id = uuid.uuid4().hex
                self._connection_started_ms = int(time.time() * 1000)
                try:
                    self.ten_env.log_info("Websocket connected successfully")
                    self.ten_env.log_info(
                        "vendor_status: websocket_connected "
                        f"ts_ms={self._connection_started_ms} internal_connection_id={self._connection_id} "
                        f"server_connect_id={self._connect_id} server_tt_logid={self._tt_logid}",
                    )
                    self.ten_env.log_debug(
                        f"vendor_status: connected to: {self.config.api_url}",
                        category=LOG_CATEGORY_VENDOR,
                    )
                    if self._session_closing:
                        self.ten_env.log_info("Session is closing, break.")
                        return

                    # Start send and receive tasks
                    self.channel_tasks = [
                        asyncio.create_task(self._send_loop(ws)),
                        asyncio.create_task(self._receive_loop(ws)),
                    ]

                    # Wait for receive loop to be ready before establishing connection
                    await self._receive_ready_event.wait()
                    connection_error = await self.start_connection()
                    if connection_error:
                        if self.on_connection_disconnected:
                            await self.on_connection_disconnected(
                                code=int(ModuleErrorCode.NON_FATAL_ERROR),
                                message=connection_error.message,
                                vendor_info=connection_error,
                            )
                        if not self._session_closing:
                            self.ten_env.log_info(
                                "Start connection failed, will reconnect.",
                            )
                            await self._reset_connection_state_for_reconnect()
                            self._connect_exp_cnt = 0
                            if self.on_connection_connecting:
                                await self.on_connection_connecting()
                            continue
                        return
                    if self.on_connection_connected:
                        await self.on_connection_connected()

                    if not self.channel_tasks:
                        return

                    # Wait for the first task to complete (sender or receiver)
                    done, pending = await asyncio.wait(
                        self.channel_tasks,
                        return_when=asyncio.FIRST_COMPLETED,
                    )
                    self.ten_env.log_info(
                        "A channel task finished, indicating connection is closing.",
                    )

                    # Cancel any remaining pending tasks
                    for task in pending:
                        task.cancel()

                    # Wait for pending tasks to be fully cancelled
                    if pending:
                        await asyncio.wait(pending)

                    # Check for exceptions in the completed tasks
                    for task in done:
                        exp = task.exception()
                        if exp and not isinstance(exp, asyncio.CancelledError):
                            self.ten_env.log_warn(
                                f"Channel task raised an exception: {exp}",
                            )
                            # Re-raise to be caught by the outer exception handlers
                            raise exp

                except websockets.ConnectionClosed as e:
                    self.ten_env.log_info(f"Websocket connection closed: {e}.")
                    if self.on_connection_disconnected:
                        await self.on_connection_disconnected(
                            code=_module_code_for_connection_closed(e),
                            message=e.reason or str(e),
                            vendor_info=ModuleErrorVendorInfo(
                                vendor=self.vendor,
                                code=str(e.code),
                                message=e.reason or str(e),
                            ),
                        )
                    self.ten_env.log_info(
                        "vendor_status: websocket_closed "
                        f"ts_ms={int(time.time() * 1000)} internal_connection_id={self._connection_id} "
                        f"server_connect_id={self._connect_id} server_tt_logid={self._tt_logid} "
                        f"code={e.code} reason={e.reason!r}",
                    )
                    if not self._session_closing:
                        self.ten_env.log_info(
                            "Websocket connection closed, will reconnect.",
                        )

                        await self._reset_connection_state_for_reconnect()

                        # Reset connection exception counter
                        self._connect_exp_cnt = 0
                        if self.on_connection_connecting:
                            await self.on_connection_connecting()
                        continue

            # If we exit the async for loop normally (not via exception),
            # it means retries were exhausted by _process_ws_exception
            if not self._session_closing:
                self.ten_env.log_error(
                    "Websocket connection loop exited, retries exhausted",
                )
                terminal_error = ModuleError(
                    code=ModuleErrorCode.NON_FATAL_ERROR.value,
                    message="Websocket connection failed: retries exhausted",
                    vendor_info=ModuleErrorVendorInfo(vendor=self.vendor),
                )

        except Exception as e:
            self.ten_env.log_error(f"Exception in websocket process: {e}")
            terminal_error = ModuleError(
                code=ModuleErrorCode.NON_FATAL_ERROR.value,
                message=f"Websocket connection failed: {e}",
                vendor_info=ModuleErrorVendorInfo(vendor=self.vendor),
            )
        finally:
            if terminal_error:
                if self.on_fatal_failure:
                    await self.on_fatal_failure(terminal_error)
                elif self.on_error:
                    await self.on_error(terminal_error)

            if self.ws:
                await self.ws.close()
            if self.on_connection_disconnected:
                disconnected_message = (
                    terminal_error.message if terminal_error else "closed"
                )
                disconnected_code = (
                    int(ModuleErrorCode.FATAL_ERROR) if terminal_error else 0
                )
                await self.on_connection_disconnected(
                    code=disconnected_code,
                    message=disconnected_message,
                    vendor_info=ModuleErrorVendorInfo(
                        vendor=self.vendor,
                        code=str(terminal_error.code) if terminal_error else "0",
                        message=disconnected_message,
                    ),
                )
            self.ten_env.log_info("Websocket connection process ended.")

    async def _send_loop(self, ws: WebSocketClientProtocol) -> None:
        """Text sending loop"""
        try:
            while not self._session_closing:
                # Get command from queue
                queue_item = await self.text_queue.get()
                if queue_item is None:  # End signal
                    break

                # Handle different types of queue items
                if isinstance(queue_item, tuple):
                    command_type, data = queue_item

                    if command_type == "text":
                        # Establish session before sending first text (if not already established)
                        if not self._session_started:
                            await self.start_session()
                            self._session_started = True

                        await self._send_text_internal(ws, data)

                    elif command_type == "finish_session":
                        # Call the actual finish_session implementation
                        await self._finish_session_internal(ws)

                else:
                    # Backward compatibility - treat as text
                    if not self._session_started:
                        await self.start_session()
                        self._session_started = True
                    await self._send_text_internal(ws, queue_item)

        except Exception as e:
            self.ten_env.log_error(f"Exception in send_loop: {e}")
            raise

    async def _receive_loop(self, ws: WebSocketClientProtocol) -> None:
        """Message receiving loop"""
        try:
            # Mark receive loop as ready
            self._receive_ready_event.set()

            async for message in ws:
                received_at = time.monotonic()
                self.ten_env.log_debug(
                    "vendor_io: websocket_receive "
                    f"ts_ms={int(time.time() * 1000)} internal_connection_id={self._connection_id} "
                    f"server_session_id={self.session_id} frame_bytes={len(message)}",
                    category=LOG_CATEGORY_VENDOR,
                )
                if self._session_closing:
                    self.ten_env.log_warn("Session is closing, break receive loop.")
                    break

                try:
                    server_response = self.handle_server_message(message)
                    if server_response:
                        server_response.received_at = received_at
                        await self._handle_server_response(server_response)
                except Exception as e:
                    self.ten_env.log_error(f"Error handling server message: {e}")

        except asyncio.CancelledError:
            self.ten_env.log_debug("Receive loop cancelled")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Exception in receive_loop: {e}")
            raise

    async def _handle_audio_payload(self, message: ServerResponse) -> None:
        if message.payload and self.response_msgs is not None:
            if self.on_audio_receive is not None and message.sessionId:
                self.on_audio_receive(
                    message.sessionId,
                    (
                        message.received_at
                        if message.received_at is not None
                        else time.monotonic()
                    ),
                )
            await self.response_msgs.put((message.event, message.payload))
        else:
            self.ten_env.log_error("Received empty payload for TTS response")

    async def _handle_server_response(self, message: ServerResponse):
        """Handle server responses"""
        self.ten_env.log_debug(
            "vendor_protocol: websocket_event "
            f"ts_ms={int(time.time() * 1000)} internal_connection_id={self._connection_id} "
            f"server_session_id={message.sessionId} event={message.event} "
            f"payload_bytes={len(message.payload) if message.payload else 0}",
            category=LOG_CATEGORY_VENDOR,
        )
        if message.event == EVENT_ConnectionStarted:
            self._connection_success = True
            self._connection_event.set()
        elif message.event == EVENT_ConnectionFailed:
            self._connection_success = False
            self._connection_event.set()
        elif message.event == EVENT_SessionStarted:
            self._session_success = True
            self._session_event.set()
            self._connection_success = True
            self._connection_event.set()
        elif message.event == EVENT_SessionFailed:
            self._session_success = False
            self._session_event.set()
        elif message.event == EVENT_TTSResponse:
            await self._handle_audio_payload(message)
        elif message.event == EVENT_TTSError:
            if self.response_msgs is not None:
                await self.response_msgs.put(
                    (
                        message.event,
                        {
                            "code": message.error_code,
                            "message": message.error_message or "Unknown error",
                            "session_id": message.sessionId,
                        },
                    ),
                )
        elif message.event == EVENT_TTSSentenceEnd:
            await self._handle_sentence_end(message)
        elif message.event == EVENT_TTSSubtitle:
            await self._forward_subtitle(message)
        elif message.event == EVENT_SessionFinished:
            if self.response_msgs is not None:
                await self.response_msgs.put(
                    (message.event, {"session_id": message.sessionId}),
                )
                self.ten_env.log_info(
                    "KEYPOINT Received usage event from Bytedance: "
                    f"session_id={message.sessionId}",
                )
                self.ten_env.log_debug(
                    f"KEYPOINT Received usage event details: {message.payload_json}",
                    category=LOG_CATEGORY_TRANSCRIPTS,
                )
                if message.payload_json and self.on_usage_characters:
                    try:
                        usage_data = json.loads(message.payload_json)
                        if (
                            "usage" in usage_data
                            and "text_words" in usage_data["usage"]
                        ):
                            char_count = int(usage_data["usage"]["text_words"])
                            self.on_usage_characters(char_count)
                    except (json.JSONDecodeError, KeyError, TypeError) as e:
                        self.ten_env.log_error(
                            f"Failed to parse usage data: error_type={type(e).__name__}",
                        )
                        self.ten_env.log_debug(
                            f"Failed to parse usage data payload: {message.payload_json}",
                            category=LOG_CATEGORY_TRANSCRIPTS,
                        )
        elif message.event == EVENT_TTSSentenceStart:
            self.ten_env.log_info(
                "KEYPOINT Received sentence start event from Bytedance: "
                f"session_id={message.sessionId}",
            )
            self.ten_env.log_debug(
                f"KEYPOINT Received sentence start event details: {message}",
                category=LOG_CATEGORY_TRANSCRIPTS,
            )
            if self.response_msgs is not None:
                await self.response_msgs.put((message.event, b""))

    async def _forward_subtitle(self, message: ServerResponse) -> None:
        subtitle = None
        if message.payload_json and self.config.enable_words:
            payload_bytes = len(message.payload_json.encode("utf-8"))
            try:
                subtitle = json.loads(message.payload_json)
                word_count = (
                    len(subtitle.get("words", [])) if isinstance(subtitle, dict) else 0
                )
                self.ten_env.log_debug(
                    "receive_words: subtitle received "
                    f"server_session_id={message.sessionId} "
                    f"payload_bytes={payload_bytes} word_count={word_count}",
                    category=LOG_CATEGORY_VENDOR,
                )
            except json.JSONDecodeError:
                self.ten_env.log_error(
                    "Failed to decode subtitle payload: "
                    f"server_session_id={message.sessionId} "
                    f"payload_bytes={payload_bytes}",
                )

        if self.response_msgs is not None:
            await self.response_msgs.put((message.event, subtitle))

    async def _handle_sentence_end(self, message: ServerResponse) -> None:
        self.ten_env.log_info(
            "KEYPOINT Received sentence end event from Bytedance: "
            f"event={message.event} session_id={message.sessionId}",
        )
        if self.response_msgs is None:
            return
        if message.payload_json and self.config.enable_words:
            await self._forward_subtitle(message)
            return
        await self.response_msgs.put((message.event, None))

    async def send_text(self, text: str):
        """Send text (external interface)"""
        await self.text_queue.put(("text", text))

    async def queue_finish_session(self):
        """Queue finish session command to ensure proper order"""
        await self.text_queue.put(("finish_session", None))

    async def _send_text_internal(self, ws: WebSocketClientProtocol, text: str):
        """Internal text sending implementation"""
        self.ten_env.log_info(
            f"KEYPOINT Sending text to Bytedance: text_length={len(text)}",
        )
        self.ten_env.log_debug(
            f"KEYPOINT Sending text to Bytedance details: {text}",
            category=LOG_CATEGORY_TRANSCRIPTS,
        )
        self.ten_env.log_debug(
            f"send_text_to_tts_server: {text} of request_id: {self.session_id}",
            category=LOG_CATEGORY_TRANSCRIPTS,
        )
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_TaskRequest,
            sessionId=self.session_id,
        ).as_bytes()
        payload = self.get_payload_bytes(
            event=EVENT_TaskRequest,
            text=text,
            speaker=self.speaker,
            model=self.model,
        )
        await self.send_event(ws, header, optional, payload, text_send=True)

    async def send_event(
        self,
        ws: WebSocketClientProtocol,
        header: bytes,
        optional: bytes | None = None,
        payload: bytes | None = None,
        *,
        text_send: bool = False,
    ):
        if ws is None:
            return
        if ws.state != State.OPEN:
            self.ten_env.log_warn("WebSocket is not open, cannot send event")
            return
        full_client_request = bytearray(header)
        if optional is not None:
            full_client_request.extend(optional)
        if payload is not None:
            payload_size = len(payload).to_bytes(4, "big", signed=False)
            full_client_request.extend(payload_size)
            full_client_request.extend(payload)
        if text_send and self.on_text_send is not None:
            self.on_text_send(self.session_id, time.monotonic())
        await ws.send(bytes(full_client_request))
        self.ten_env.log_debug(
            "vendor_io: websocket_send "
            f"ts_ms={int(time.time() * 1000)} internal_connection_id={self._connection_id} "
            f"server_session_id={self.session_id} frame_bytes={len(full_client_request)}",
            category=LOG_CATEGORY_VENDOR,
        )

    async def start_connection(self) -> ModuleErrorVendorInfo | None:
        self.ten_env.log_info("KEYPOINT Starting connection")
        # Reset connection event
        self._connection_event.clear()
        self._connection_success = False

        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(event=EVENT_Start_Connection).as_bytes()
        payload = b"{}"
        await self.send_event(self.ws, header, optional, payload)

        # Wait for connection response
        try:
            await asyncio.wait_for(self._connection_event.wait(), timeout=10.0)
            if not self._connection_success:
                error_info = ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code="CONNECTION_FAILED",
                    message="Start connection failed",
                )
                if self.on_error:
                    await self.on_error(
                        ModuleError(
                            code=ModuleErrorCode.NON_FATAL_ERROR.value,
                            message=error_info.message,
                            vendor_info=error_info,
                        ),
                    )
                return error_info
        except asyncio.TimeoutError:
            error_info = ModuleErrorVendorInfo(
                vendor=self.vendor,
                code="TIMEOUT",
                message="Start connection timeout",
            )
            if self.on_error:
                await self.on_error(
                    ModuleError(
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message=error_info.message,
                        vendor_info=error_info,
                    ),
                )
            return error_info
        return None

    async def _reset_connection_state_for_reconnect(self) -> None:
        if self.channel_tasks:
            for task in self.channel_tasks:
                task.cancel()
            await asyncio.wait(self.channel_tasks)
            self.channel_tasks.clear()

        self._receive_ready_event.clear()
        self._connection_event.clear()
        self._session_event.clear()
        self._connection_success = False
        self._session_success = False
        self._session_started = False

    async def start_session(self):
        self.ten_env.log_info("KEYPOINT start session")
        # Reset session event
        self._session_event.clear()
        self._session_success = False

        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_StartSession,
            sessionId=self.session_id,
        ).as_bytes()
        payload = self.get_payload_bytes(
            event=EVENT_StartSession,
            speaker=self.speaker,
            model=self.model,
        )
        await self.send_event(self.ws, header, optional, payload)

        # Wait for session response
        try:
            await asyncio.wait_for(self._session_event.wait(), timeout=10.0)
            if not self._session_success:
                error_info = ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code="SESSION_FAILED",
                    message="Start session failed",
                )
                if self.on_error:
                    await self.on_error(
                        ModuleError(
                            code=ModuleErrorCode.NON_FATAL_ERROR.value,
                            message=error_info.message,
                            vendor_info=error_info,
                        ),
                    )
        except asyncio.TimeoutError:
            error_info = ModuleErrorVendorInfo(
                vendor=self.vendor,
                code="TIMEOUT",
                message="Start session timeout",
            )
            if self.on_error:
                await self.on_error(
                    ModuleError(
                        code=ModuleErrorCode.NON_FATAL_ERROR.value,
                        message=error_info.message,
                        vendor_info=error_info,
                    ),
                )

    async def _finish_session_internal(self, ws: WebSocketClientProtocol):
        """Internal finish session implementation"""
        self.ten_env.log_info("KEYPOINT finish session internal")
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_FinishSession,
            sessionId=self.session_id,
        ).as_bytes()
        await self.send_event(ws, header, optional, b"{}")
        # Reset session state, next text sending will re-establish session
        self._session_started = False
        self.ten_env.log_info("KEYPOINT Session finished successfully")

    async def finish_session(self):
        """Public finish session method - queues the command for proper ordering"""
        await self.queue_finish_session()

    async def finish_connection(self):
        self.ten_env.log_info("KEYPOINT finish connection")
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(event=EVENT_FinishConnection).as_bytes()
        await self.send_event(self.ws, header, optional, b"{}")

    def handle_server_message(self, message: str) -> ServerResponse:
        try:
            return self.parse_server_message(message)
        except Exception as e:
            self.ten_env.log_error(f"Error handling message {e}")
            return None

    def parse_server_message(self, res) -> ServerResponse:
        try:
            response = parser_response(res)
            self.ten_env.log_debug(
                "vendor_protocol: websocket_frame "
                f"ts_ms={int(time.time() * 1000)} internal_connection_id={self._connection_id} "
                f"message_type={response.header.message_type} "
                f"flags={response.header.message_type_specific_flags} "
                f"serialization={response.header.serial_method} "
                f"compression={response.header.compression_type} "
                f"event={response.optional.event} server_session_id={response.optional.sessionId} "
                f"error_code={response.optional.errorCode} "
                f"payload_bytes={len(response.payload) if response.payload else 0}",
                category=LOG_CATEGORY_VENDOR,
            )
            if response.header.message_type in (
                FULL_SERVER_RESPONSE,
                AUDIO_ONLY_RESPONSE,
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
            if response.header.message_type == ERROR_INFORMATION:
                # Try to parse error payload
                error_message = "Unknown error"
                if response.payload:
                    try:
                        error_message = response.payload.decode("utf-8")
                        self.ten_env.log_error(
                            "KEYPOINT decoded error payload: "
                            f"code={response.optional.errorCode}",
                        )
                        self.ten_env.log_error(
                            f"vendor_error: code: {response.optional.errorCode}",
                            category=LOG_CATEGORY_VENDOR,
                        )
                        self.ten_env.log_debug(
                            f"vendor_error details: {error_message}",
                            category=LOG_CATEGORY_TRANSCRIPTS,
                        )
                    except Exception as e:
                        self.ten_env.log_error(f"Failed to decode error payload: {e}")
                        error_message = (
                            f"Binary payload (length: {len(response.payload)})"
                        )

                return ServerResponse(
                    event=EVENT_TTSError,
                    sessionId=self.session_id,
                    error_code=response.optional.errorCode,
                    error_message=error_message,
                )
            raise RuntimeError(
                f"unknown message type: {response.header.message_type}",
            )
        except json.JSONDecodeError as e:
            self.ten_env.log_error(f"Failed to parse server message: {e}")
            raise RuntimeError(f"Failed to parse server message: {e}") from e

    def _print_response(self, res: ServerResponse, tag: str):
        self.ten_env.log_debug(f"[{tag}] Header: {res.header}")
        self.ten_env.log_debug(f"[{tag}] Optional: {res.optional}")
        self.ten_env.log_debug(
            f"[{tag}] Payload Len: {len(res.payload) if res.payload else 0}",
        )
        self.ten_env.log_debug(
            f"[{tag}] Payload JSON: {res.payload_json}",
            category=LOG_CATEGORY_TRANSCRIPTS,
        )

    def cancel(self) -> None:
        """Cancel current connection, used for flush scenarios"""
        self.ten_env.log_info("Cancelling the request.")

        # The websocket connection might be not established yet, if so, using
        # this flag to close the connection directly.
        self._session_closing = True

        # Note that the websocket connection might not be established yet
        # (i.e., self.channel_tasks is empty).
        for task in self.channel_tasks:
            task.cancel()

        # Clear all queues to prevent old data from being processed
        self._clear_queues()

        # We do not wait the websocket_task to be completed, as the duration
        # of closing the websocket might be more than 10 seconds by default.
        #
        # After the sender/receiver tasks are completed, `self._process_websocket()`
        # should be quit soon, and then `close()` will be called on the
        # websocket connection at exit of function. So the websocket connection
        # will be closed eventually.

    async def suppress_connection_callbacks_for_replacement(self) -> None:
        if self.on_connection_disconnected and self.channel_tasks:
            await self.on_connection_disconnected(
                code=0,
                message="closed",
                vendor_info=ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code="0",
                    message="closed",
                ),
            )
        self.on_connection_connecting = None
        self.on_connection_connected = None
        self.on_connection_disconnected = None

    def _clear_queues(self) -> None:
        """Clear all queues to prevent old data from being processed"""
        # Clear text queue
        while not self.text_queue.empty():
            try:
                self.text_queue.get_nowait()
            except asyncio.QueueEmpty:
                break

        # Clear response messages queue
        if self.response_msgs:
            while not self.response_msgs.empty():
                try:
                    self.response_msgs.get_nowait()
                except asyncio.QueueEmpty:
                    break

        self.ten_env.log_info("All queues cleared during cancel")

    async def close(self):
        self.ten_env.log_info("Closing BytedanceV3Synthesizer")

        # Set closing flag
        self._session_closing = True

        # Send end signal to text queue
        await self.text_queue.put(None)

        # Cancel websocket task
        if self.websocket_task:
            self.websocket_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self.websocket_task

        # Close websocket connection
        if self.ws:
            await self.ws.close()
            self.ws = None
        self.response_msgs = None


class BytedanceV3Client:
    def __init__(
        self,
        config: BytedanceTTSDuplexConfig,
        ten_env: AsyncTenEnv,
        vendor: str,
        response_msgs: asyncio.Queue[tuple[int, bytes | dict | None]],
        on_error: Callable[[ModuleError], Awaitable[None]] | None = None,
        on_usage_characters: Callable[[int], None] | None = None,
        on_fatal_failure: Callable[[ModuleError], Awaitable[None]] | None = None,
        on_connection_connecting: Callable[[], Awaitable[None]] | None = None,
        on_connection_connected: Callable[[], Awaitable[None]] | None = None,
        on_connection_disconnected: Callable[..., Awaitable[None]] | None = None,
        on_text_send: Callable[[str, float], None] | None = None,
        on_audio_receive: Callable[[str, float], None] | None = None,
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.response_msgs = response_msgs
        self.on_error = on_error
        self.on_usage_characters = on_usage_characters
        self.on_fatal_failure = on_fatal_failure
        self.on_connection_connecting = on_connection_connecting
        self.on_connection_connected = on_connection_connected
        self.on_connection_disconnected = on_connection_disconnected
        self.on_text_send = on_text_send
        self.on_audio_receive = on_audio_receive

        # Current active synthesizer
        self.synthesizer: BytedanceV3Synthesizer = self._create_synthesizer()

        # List of synthesizers to be cleaned up
        self.cancelled_synthesizers = []

        # Cleanup task
        self.cleanup_task = asyncio.create_task(self._cleanup_cancelled_synthesizers())

    @property
    def session_id(self) -> str:
        return self.synthesizer.session_id

    def _create_synthesizer(self) -> BytedanceV3Synthesizer:
        """Create new synthesizer instance"""
        return BytedanceV3Synthesizer(
            self.config,
            self.ten_env,
            self.vendor,
            self.response_msgs,
            self.on_error,
            self.on_usage_characters,
            self.on_fatal_failure,
            self.on_connection_connecting,
            self.on_connection_connected,
            self.on_connection_disconnected,
            self.on_text_send,
            self.on_audio_receive,
        )

    def update_config(self, config: BytedanceTTSDuplexConfig) -> None:
        """Update the configuration used for future synthesizer instances."""
        self.config = config
        if self.ten_env:
            self.ten_env.log_info("Manager configuration updated.")

    async def _cleanup_cancelled_synthesizers(self) -> None:
        """Periodically clean up completed cancelled synthesizers"""
        while True:
            try:
                for synthesizer in self.cancelled_synthesizers[:]:
                    if synthesizer.websocket_task and synthesizer.websocket_task.done():
                        self.ten_env.log_info(
                            f"Cleaning up cancelled synthesizer {id(synthesizer)}",
                        )
                        self.cancelled_synthesizers.remove(synthesizer)

                await asyncio.sleep(5.0)  # Check every 5 seconds
            except Exception as e:
                self.ten_env.log_error(f"Error in cleanup task: {e}")
                await asyncio.sleep(5.0)

    async def cancel(self) -> None:
        """Cancel current synthesizer and create new synthesizer"""
        self.ten_env.log_info("Cancelling current synthesizer and creating new one")

        # Clear response messages queue to prevent old data from being processed
        if self.response_msgs:
            while not self.response_msgs.empty():
                try:
                    self.response_msgs.get_nowait()
                except asyncio.QueueEmpty:
                    break
            self.ten_env.log_info("Response messages queue cleared during cancel")

        # Move current synthesizer to cleanup list
        if self.synthesizer:
            self.cancelled_synthesizers.append(self.synthesizer)
            await self.synthesizer.suppress_connection_callbacks_for_replacement()
            self.synthesizer.cancel()

        # Create new synthesizer
        self.synthesizer = self._create_synthesizer()
        self.ten_env.log_info("New synthesizer created successfully")

    async def reset_synthesizer(self):
        """Reset synthesizer"""
        if self.synthesizer:
            self.cancelled_synthesizers.append(self.synthesizer)
            await self.synthesizer.suppress_connection_callbacks_for_replacement()
            self.synthesizer.cancel()

        self.synthesizer = self._create_synthesizer()
        self.ten_env.log_info("Synthesizer reset successfully")

    async def send_text(self, text: str):
        """Send text"""
        await self.synthesizer.send_text(text)

    async def finish_session(self):
        """Finish session"""
        await self.synthesizer.finish_session()

    async def finish_connection(self):
        """Finish connection"""
        await self.synthesizer.finish_connection()

    async def close(self):
        """Close client"""
        self.ten_env.log_info("Closing BytedanceV3Client")

        # Cancel cleanup task
        if self.cleanup_task:
            self.cleanup_task.cancel()
            with contextlib.suppress(asyncio.CancelledError):
                await self.cleanup_task

        # Close current synthesizer
        if self.synthesizer:
            await self.synthesizer.close()

        # Close all cancelled synthesizers
        for synthesizer in self.cancelled_synthesizers:
            try:
                await synthesizer.close()
            except Exception as e:
                self.ten_env.log_error(f"Error closing cancelled synthesizer: {e}")

        self.cancelled_synthesizers.clear()
        self.ten_env.log_info("BytedanceV3Client closed")
