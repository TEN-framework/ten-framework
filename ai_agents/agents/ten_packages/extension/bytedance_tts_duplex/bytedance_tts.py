import asyncio
import json
from typing import Tuple
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

# Connection-related constants
MAX_RETRY_TIMES_FOR_TRANSPORT = 5
ERR_WS_CONNECTION = 0

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
        return bytes(option_bytes)


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


class BytedanceV3Synthesizer:
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
        self.speaker = config.speaker
        self.session_id = uuid.uuid4().hex
        self.ws: WebSocketClientProtocol = None
        self.stop_event = asyncio.Event()
        self.ten_env: AsyncTenEnv = ten_env
        self.vendor = vendor
        self.response_msgs: asyncio.Queue[Tuple[int, bytes, bool]] | None = (
            response_msgs
        )

        # Connection management related
        self._session_closing = False
        self._connect_exp_cnt = 0
        self.websocket_task = None
        self.channel_tasks = []
        self._session_started = False

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

    def _process_ws_exception(self, exp) -> None | Exception:
        """Handle websocket connection exceptions and decide whether to reconnect"""
        self.ten_env.log_warn(
            f"Websocket internal error during connecting: {exp}."
        )
        self._connect_exp_cnt += 1
        if self._connect_exp_cnt > MAX_RETRY_TIMES_FOR_TRANSPORT:
            self.ten_env.log_error(
                f"Max retries ({MAX_RETRY_TIMES_FOR_TRANSPORT}) exceeded: {str(exp)}"
            )
            return exp
        return None  # Return None to continue reconnection

    async def _process_websocket(self) -> None:
        """Main websocket connection monitoring and reconnection logic"""
        try:
            self.ten_env.log_info("Starting websocket connection process")
            # Use websockets.connect's automatic reconnection mechanism
            async for ws in websockets.connect(
                uri=self.config.api_url,
                additional_headers=self.get_headers(),
                max_size=100_000_000,
                compression=None,
                process_exception=self._process_ws_exception,
            ):
                self.ws = ws
                try:
                    self.ten_env.log_info("Websocket connected successfully")
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
                    await self.start_connection()

                    if not self.channel_tasks:
                        return

                    # Wait for the first task to complete (sender or receiver)
                    (done, pending) = await asyncio.wait(
                        self.channel_tasks,
                        return_when=asyncio.FIRST_COMPLETED,
                    )
                    self.ten_env.log_info(
                        "A channel task finished, indicating connection is closing."
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
                                f"Channel task raised an exception: {exp}"
                            )
                            # Re-raise to be caught by the outer exception handlers
                            raise exp

                except websockets.ConnectionClosed as e:
                    self.ten_env.log_info(f"Websocket connection closed: {e}.")
                    if not self._session_closing:
                        self.ten_env.log_info(
                            "Websocket connection closed, will reconnect."
                        )

                        # Cancel all channel tasks and wait for them to finish
                        if self.channel_tasks:
                            for task in self.channel_tasks:
                                task.cancel()
                            # Wait for tasks to complete their cancellation
                            await asyncio.wait(self.channel_tasks)
                            self.channel_tasks.clear()

                        # Reset all event states
                        self._receive_ready_event.clear()
                        self._connection_event.clear()
                        self._session_event.clear()
                        self._connection_success = False
                        self._session_success = False
                        self._session_started = False

                        # Reset connection exception counter
                        self._connect_exp_cnt = 0
                        continue

        except Exception as e:
            self.ten_env.log_error(f"Exception in websocket process: {e}")
        finally:
            if self.ws:
                await self.ws.close()
            self.ten_env.log_info("Websocket connection process ended.")

    async def _establish_connection_and_session(self):
        """Establish connection and session"""
        await self.start_connection()
        await self.start_session()

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
            raise e

    async def _receive_loop(self, ws: WebSocketClientProtocol) -> None:
        """Message receiving loop"""
        try:
            # Mark receive loop as ready
            self._receive_ready_event.set()

            async for message in ws:
                if self._session_closing:
                    self.ten_env.log_warn(
                        "Session is closing, break receive loop."
                    )
                    break

                try:
                    server_response = self.handle_server_message(message)
                    if server_response:
                        await self._handle_server_response(server_response)
                except Exception as e:
                    self.ten_env.log_error(
                        f"Error handling server message: {e}"
                    )

        except asyncio.CancelledError:
            self.ten_env.log_debug("Receive loop cancelled")
            raise
        except Exception as e:
            self.ten_env.log_error(f"Exception in receive_loop: {e}")
            raise e

    async def _handle_server_response(self, message: ServerResponse):
        """Handle server responses"""
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
            if message.payload and self.response_msgs is not None:
                await self.response_msgs.put((message.event, message.payload))
            else:
                self.ten_env.log_error(
                    "Received empty payload for TTS response"
                )
        elif message.event == EVENT_TTSSentenceEnd:
            if self.response_msgs is not None:
                await self.response_msgs.put((message.event, b""))
        elif message.event == EVENT_SessionFinished:
            if self.response_msgs is not None:
                await self.response_msgs.put((message.event, b""))

    async def send_text(self, text: str):
        """Send text (external interface)"""
        await self.text_queue.put(("text", text))

    async def queue_finish_session(self):
        """Queue finish session command to ensure proper order"""
        await self.text_queue.put(("finish_session", None))

    async def _send_text_internal(self, ws: WebSocketClientProtocol, text: str):
        """Internal text sending implementation"""
        self.ten_env.log_info(f"KEYPOINT Sending text to Bytedance: {text}")
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
        await self.send_event(ws, header, optional, payload)

    async def send_event(
        self,
        ws: WebSocketClientProtocol,
        header: bytes,
        optional: bytes | None = None,
        payload: bytes = None,
    ):
        if ws is not None:
            if ws.state != State.OPEN:
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
                await ws.send(bytes(full_client_request))

    async def start_connection(self):
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
                raise ModuleVendorException(
                    ModuleErrorVendorInfo(
                        vendor=self.vendor,
                        code="CONNECTION_FAILED",
                        message="Start connection failed",
                    )
                )
        except asyncio.TimeoutError as exc:
            raise ModuleVendorException(
                ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code="TIMEOUT",
                    message="Start connection timeout",
                )
            ) from exc

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
            event=EVENT_StartSession, sessionId=self.session_id
        ).as_bytes()
        payload = self.get_payload_bytes(
            event=EVENT_StartSession, speaker=self.speaker
        )
        await self.send_event(self.ws, header, optional, payload)

        # Wait for session response
        try:
            await asyncio.wait_for(self._session_event.wait(), timeout=10.0)
            if not self._session_success:
                raise ModuleVendorException(
                    ModuleErrorVendorInfo(
                        vendor=self.vendor,
                        code="SESSION_FAILED",
                        message="Start session failed",
                    )
                )
        except asyncio.TimeoutError as exc:
            raise ModuleVendorException(
                ModuleErrorVendorInfo(
                    vendor=self.vendor,
                    code="TIMEOUT",
                    message="Start session timeout",
                )
            ) from exc

    async def _finish_session_internal(self, ws: WebSocketClientProtocol):
        """Internal finish session implementation"""
        self.ten_env.log_info("KEYPOINT finish session internal")
        header = Header(
            message_type=FULL_CLIENT_REQUEST,
            message_type_specific_flags=MsgTypeFlagWithEvent,
            serial_method=JSON,
        ).as_bytes()
        optional = Optional(
            event=EVENT_FinishSession, sessionId=self.session_id
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
                # Try to parse error payload
                error_message = "Unknown error"
                if response.payload:
                    try:
                        error_message = response.payload.decode("utf-8")
                        self.ten_env.log_error(
                            f"KEYPOINT decoded error payload: {error_message}"
                        )
                    except Exception as e:
                        self.ten_env.log_error(
                            f"Failed to decode error payload: {e}"
                        )
                        error_message = (
                            f"Binary payload (length: {len(response.payload)})"
                        )

                raise ModuleVendorException(
                    ModuleErrorVendorInfo(
                        vendor=self.vendor,
                        code=str(response.optional.errorCode),
                        message=error_message,
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
            try:
                await self.websocket_task
            except asyncio.CancelledError:
                pass

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
        response_msgs: asyncio.Queue[Tuple[int, bytes]],
    ):
        self.config = config
        self.ten_env = ten_env
        self.vendor = vendor
        self.response_msgs = response_msgs

        # Current active synthesizer
        self.synthesizer: BytedanceV3Synthesizer = self._create_synthesizer()

        # List of synthesizers to be cleaned up
        self.cancelled_synthesizers = []

        # Cleanup task
        self.cleanup_task = asyncio.create_task(
            self._cleanup_cancelled_synthesizers()
        )

    def _create_synthesizer(self) -> BytedanceV3Synthesizer:
        """Create new synthesizer instance"""
        return BytedanceV3Synthesizer(
            self.config, self.ten_env, self.vendor, self.response_msgs
        )

    async def _cleanup_cancelled_synthesizers(self) -> None:
        """Periodically clean up completed cancelled synthesizers"""
        while True:
            try:
                for synthesizer in self.cancelled_synthesizers[:]:
                    if (
                        synthesizer.websocket_task
                        and synthesizer.websocket_task.done()
                    ):
                        self.ten_env.log_info(
                            f"Cleaning up cancelled synthesizer {id(synthesizer)}"
                        )
                        self.cancelled_synthesizers.remove(synthesizer)

                await asyncio.sleep(5.0)  # Check every 5 seconds
            except Exception as e:
                self.ten_env.log_error(f"Error in cleanup task: {e}")
                await asyncio.sleep(5.0)

    def cancel(self) -> None:
        """Cancel current synthesizer and create new synthesizer"""
        self.ten_env.log_info(
            "Cancelling current synthesizer and creating new one"
        )

        # Clear response messages queue to prevent old data from being processed
        if self.response_msgs:
            while not self.response_msgs.empty():
                try:
                    self.response_msgs.get_nowait()
                except asyncio.QueueEmpty:
                    break
            self.ten_env.log_info(
                "Response messages queue cleared during cancel"
            )

        # Move current synthesizer to cleanup list
        if self.synthesizer:
            self.cancelled_synthesizers.append(self.synthesizer)
            self.synthesizer.cancel()

        # Create new synthesizer
        self.synthesizer = self._create_synthesizer()
        self.ten_env.log_info("New synthesizer created successfully")

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
            try:
                await self.cleanup_task
            except asyncio.CancelledError:
                pass

        # Close current synthesizer
        if self.synthesizer:
            await self.synthesizer.close()

        # Close all cancelled synthesizers
        for synthesizer in self.cancelled_synthesizers:
            try:
                await synthesizer.close()
            except Exception as e:
                self.ten_env.log_error(
                    f"Error closing cancelled synthesizer: {e}"
                )

        self.cancelled_synthesizers.clear()
        self.ten_env.log_info("BytedanceV3Client closed")
