import asyncio
import json
import time
import base64
import os
import audioop
from datetime import datetime
from typing import Literal, Dict, Any, Optional
import websockets

from .agent.decorators import agent_event_handler
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    Data,
    AudioFrame,
    Loc,
)
from ten_runtime.audio_frame import AudioFrameDataFmt

from .agent.agent import Agent
from .agent.events import (
    ASRResultEvent,
    LLMResponseEvent,
    ToolRegisterEvent,
    UserJoinedEvent,
    UserLeftEvent,
)
from .helper import _send_cmd, _send_data, parse_sentences
from .config import MainControlConfig

import uuid


class MainControlExtension(AsyncExtension):
    """
    The entry point of the agent module.
    Consumes semantic AgentEvents from the Agent class and drives the runtime behavior.
    """

    def __init__(self, name: str):
        super().__init__(name)
        self.ten_env: AsyncTenEnv = None
        self.agent: Agent = None
        self.config: MainControlConfig = None

        # WebSocket and audio processing
        self.active_call_sessions: Dict[str, Dict[str, Any]] = {}
        self.audio_dump_files: Dict[str, str] = {}  # call_sid -> filepath
        self.audio_dump_dir: str = ""

        self.stopped: bool = False
        self._rtc_user_count: int = 0
        self.sentence_fragment: str = ""
        self.turn_id: int = 0
        self.session_id: str = "0"

    def _current_metadata(self) -> dict:
        return {"session_id": self.session_id, "turn_id": self.turn_id}

    async def on_init(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env

        # Load config from runtime properties
        config_json, _ = await ten_env.get_property_to_json(None)

        self.ten_env.log_info(f"Config12: {config_json}")

        self.config = MainControlConfig.model_validate_json(config_json)

        self.ten_env.log_info(f"Config11: {self.config}")

        self.agent = Agent(ten_env)

        # Now auto-register decorated methods
        for attr_name in dir(self):
            fn = getattr(self, attr_name)
            event_type = getattr(fn, "_agent_event_type", None)
            if event_type:
                self.agent.on(event_type, fn)

    # === Register handlers with decorators ===
    @agent_event_handler(UserJoinedEvent)
    async def _on_user_joined(self, event: UserJoinedEvent):
        self._rtc_user_count += 1
        if self._rtc_user_count == 1 and self.config and self.config.greeting:
            await self._send_to_tts(self.config.greeting, True)
            await self._send_transcript(
                "assistant", self.config.greeting, True, 100
            )

    @agent_event_handler(UserLeftEvent)
    async def _on_user_left(self, event: UserLeftEvent):
        self._rtc_user_count -= 1

    @agent_event_handler(ToolRegisterEvent)
    async def _on_tool_register(self, event: ToolRegisterEvent):
        await self.agent.register_llm_tool(event.tool, event.source)

    @agent_event_handler(ASRResultEvent)
    async def _on_asr_result(self, event: ASRResultEvent):
        self.ten_env.log_info(f"[MainControlExtension] ASR Result: {event.text}")
        self.session_id = event.metadata.get("session_id", "100")
        stream_id = int(self.session_id)
        if not event.text:
            return
        if event.final or len(event.text) > 2:
            await self._interrupt()
        if event.final:
            self.turn_id += 1
            await self.agent.queue_llm_input(event.text)
        await self._send_transcript("user", event.text, event.final, stream_id)

    @agent_event_handler(LLMResponseEvent)
    async def _on_llm_response(self, event: LLMResponseEvent):
        if not event.is_final and event.type == "message":
            sentences, self.sentence_fragment = parse_sentences(
                self.sentence_fragment, event.delta
            )
            for s in sentences:
                await self._send_to_tts(s, False)

        if event.is_final and event.type == "message":
            remaining_text = self.sentence_fragment or ""
            self.sentence_fragment = ""
            await self._send_to_tts(remaining_text, True)

        await self._send_transcript(
            "assistant",
            event.text,
            event.is_final,
            100,
            data_type=("reasoning" if event.type == "reasoning" else "text"),
        )

    async def on_start(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_start")

        # Initialize WebSocket and audio processing
        if self.config:
            self._setup_audio_dump_directory()

            # Start WebSocket server in background
            asyncio.create_task(self._start_websocket_server())
            ten_env.log_info(
                f"WebSocket server started on port {self.config.twilio_server_media_ws_port}"
            )

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True
        await self.agent.stop()

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd):
        await self.agent.on_cmd(cmd)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data):
        await self.agent.on_data(data)

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, audio_frame: AudioFrame
    ) -> None:
        """Handle outgoing audio frames from TEN framework"""
        try:
            if self.twilio_server and audio_frame.get_name() == "audio_out":
                audio_data = audio_frame.get_buf()
                # Send audio to all active Twilio calls
                for call_sid in self.twilio_server.active_call_sessions.keys():
                    await self.twilio_server.send_audio_to_twilio(
                        audio_data, call_sid
                    )
        except Exception as e:
            ten_env.log_error(f"Failed to handle audio frame: {e}")

    # === helpers ===
    async def _send_transcript(
        self,
        role: str,
        text: str,
        final: bool,
        stream_id: int,
        data_type: Literal["text", "reasoning"] = "text",
    ):
        """
        Sends the transcript (ASR or LLM output) to the message collector.
        """
        if data_type == "text":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "transcribe",
                    "role": role,
                    "text": text,
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        elif data_type == "reasoning":
            await _send_data(
                self.ten_env,
                "message",
                "message_collector",
                {
                    "data_type": "raw",
                    "role": role,
                    "text": json.dumps(
                        {
                            "type": "reasoning",
                            "data": {
                                "text": text,
                            },
                        }
                    ),
                    "text_ts": int(time.time() * 1000),
                    "is_final": final,
                    "stream_id": stream_id,
                },
            )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent transcript: {role}, final={final}, text={text}"
        )

    async def _send_to_tts(self, text: str, is_final: bool):
        """
        Sends a sentence to the TTS system.
        """
        request_id = f"tts-request-{self.turn_id}"
        await _send_data(
            self.ten_env,
            "tts_text_input",
            "tts",
            {
                "request_id": request_id,
                "text": text,
                "text_input_end": is_final,
                "metadata": self._current_metadata(),
            },
        )
        self.ten_env.log_info(
            f"[MainControlExtension] Sent to TTS: is_final={is_final}, text={text}"
        )

    async def _interrupt(self):
        """
        Interrupts ongoing LLM and TTS generation. Typically called when user speech is detected.
        """
        self.sentence_fragment = ""
        await self.agent.flush_llm()
        await _send_data(
            self.ten_env, "tts_flush", "tts", {"flush_id": str(uuid.uuid4())}
        )
        await _send_cmd(self.ten_env, "flush", "agora_rtc")
        self.ten_env.log_info("[MainControlExtension] Interrupt signal sent")

    # WebSocket and audio processing methods
    def _setup_audio_dump_directory(self):
        """Setup directory for audio dump files"""
        # Use configured directory or default
        self.audio_dump_dir = getattr(self.config, 'audio_dump_directory', "/tmp/twilio_audio_dumps")
        os.makedirs(self.audio_dump_dir, exist_ok=True)
        if self.ten_env:
            self.ten_env.log_info(f"Audio dump directory created: {self.audio_dump_dir}")

    async def _forward_audio_to_ten(self, audio_payload: str, call_sid: str):
        """Forward audio data to TEN framework and dump PCM audio"""
        try:
            if not self.ten_env:
                return

            # Decode base64 audio data (this is μ-law encoded)
            mulaw_data = base64.b64decode(audio_payload)

            # Convert μ-law to PCM
            pcm_data = audioop.ulaw2lin(mulaw_data, 2)  # 2 bytes per sample (16-bit)

            # Dump PCM audio to file
            await self._dump_pcm_audio(pcm_data, call_sid)

            # Create AudioFrame and send to TEN framework
            audio_frame = AudioFrame.create("pcm_frame")
            audio_frame.alloc_buf(len(pcm_data))
            buf = audio_frame.lock_buf()
            buf[:] = pcm_data
            audio_frame.unlock_buf(buf)
            audio_frame.set_sample_rate(8000)
            audio_frame.set_number_of_channels(1)
            audio_frame.set_bytes_per_sample(2)
            audio_frame.set_data_fmt(AudioFrameDataFmt.INTERLEAVE)
            audio_frame.set_samples_per_channel(len(pcm_data) // (2 * 1))
            audio_frame.set_property_int("stream_id", 54321)
            audio_frame.set_dests([Loc(app_uri="", graph_id="", extension_name="streamid_adapter")])

            await self.ten_env.send_audio_frame(audio_frame)

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to forward audio to TEN: {e}")

    def _init_audio_dump_file(self, call_sid: str):
        """Initialize audio dump file for a call"""
        try:
            if call_sid in self.audio_dump_files:
                return  # File already initialized

            # Create filename with timestamp and call_sid
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"twilio_audio_{call_sid}_{timestamp}.pcm"
            filepath = os.path.join(self.audio_dump_dir, filename)

            # Create empty file to initialize
            with open(filepath, 'wb') as f:
                pass  # Create empty file

            self.audio_dump_files[call_sid] = filepath

            if self.ten_env:
                self.ten_env.log_info(f"Initialized audio dump file: {filepath}")

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to initialize audio dump file: {e}")

    async def _dump_pcm_audio(self, audio_data: bytes, call_sid: str):
        """Dump PCM audio data to file"""
        try:
            # Initialize file if not exists
            if call_sid not in self.audio_dump_files:
                self._init_audio_dump_file(call_sid)

            # Get filepath for this call
            filepath = self.audio_dump_files.get(call_sid)
            if not filepath:
                return

            # Write PCM audio data to file
            with open(filepath, 'ab') as f:  # 'ab' mode for appending binary data
                f.write(audio_data)

            if self.ten_env:
                self.ten_env.log_info(f"Dumped {len(audio_data)} bytes of PCM audio (converted from μ-law) to {filepath}")

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to dump PCM audio: {e}")

    async def send_audio_to_twilio(self, audio_data: bytes, call_sid: str):
        """Send audio data to Twilio via WebSocket"""
        try:
            if call_sid not in self.active_call_sessions:
                return

            websocket = self.active_call_sessions[call_sid].get("websocket")
            if not websocket:
                return

            # Convert PCM to μ-law for Twilio
            mulaw_data = audioop.lin2ulaw(audio_data, 2)  # 2 bytes per sample (16-bit)

            # Encode μ-law audio data to base64
            audio_base64 = base64.b64encode(mulaw_data).decode("utf-8")

            message = {
                "event": "media",
                "streamSid": call_sid,
                "media": {"payload": audio_base64},
            }

            await websocket.send_text(json.dumps(message))

        except Exception as e:
            if self.ten_env:
                self.ten_env.log_error(f"Failed to send audio to Twilio: {e}")

    async def _cleanup_call_after_delay(self, call_sid: str, delay_seconds: int):
        """Clean up call session after a delay"""
        await asyncio.sleep(delay_seconds)
        if call_sid in self.active_call_sessions:
            del self.active_call_sessions[call_sid]
            if self.ten_env:
                self.ten_env.log_info(f"Cleaned up call session: {call_sid}")

        # Clean up audio dump file
        if call_sid in self.audio_dump_files:
            filepath = self.audio_dump_files[call_sid]
            del self.audio_dump_files[call_sid]
            if self.ten_env:
                self.ten_env.log_info(f"Audio dump file ready: {filepath}")

    async def _start_websocket_server(self):
        """Start the WebSocket server"""
        async def websocket_handler(websocket):
            """Handle WebSocket connections for Twilio media streaming"""
            stream_sid = None
            call_sid = None

            if self.ten_env:
                self.ten_env.log_info("WebSocket connection established with Twilio")

            try:
                # Wait for start event (may receive connected event first)
                while True:
                    message = await websocket.recv()
                    message = json.loads(message)

                    if message.get("event") == "connected":
                        if self.ten_env:
                            self.ten_env.log_info(f"WebSocket connected: {message}")
                        continue  # Wait for start event

                    elif "start" in message:
                        stream_sid = message["start"]["streamSid"]
                        call_sid = message["start"]["callSid"]

                        if self.ten_env:
                            self.ten_env.log_info(f"Received start event for streamSid: {stream_sid}, callSid: {call_sid}")

                        # Store WebSocket connection in active session
                        if call_sid in self.active_call_sessions:
                            self.active_call_sessions[call_sid]["websocket"] = websocket
                        else:
                            # Create new session if not exists
                            self.active_call_sessions[call_sid] = {
                                "websocket": websocket,
                                "stream_sid": stream_sid,
                                "created_at": time.time(),
                            }
                        break  # Exit the start event waiting loop

                    else:
                        if self.ten_env:
                            self.ten_env.log_warn(f"Received unexpected event while waiting for start: {message}")
                        return

                # Process incoming messages after start event
                while True:
                    message = await websocket.recv()
                    message = json.loads(message)

                    self.ten_env.log_info(f"Received message: {message}")

                    if message["event"] == "media":
                        # Handle incoming audio data
                        payload = message["media"]["payload"]
                        if self.ten_env:
                            self.ten_env.log_debug(f"Received audio data: {len(payload)} bytes")

                        # Forward audio to TEN framework
                        await self._forward_audio_to_ten(payload, call_sid)

                    elif message["event"] == "stop":
                        if self.ten_env:
                            self.ten_env.log_info(f"Received stop event for stream {stream_sid}")
                        break

                    elif message["event"] == "mark":
                        if self.ten_env:
                            self.ten_env.log_info(f"Received mark event for stream {stream_sid}: {message['mark']['name']}")

                    else:
                        if self.ten_env:
                            self.ten_env.log_info(f"Received unknown event: {message['event']}")

            except Exception as e:
                if self.ten_env:
                    self.ten_env.log_error(f"WebSocket error: {e}")
            finally:
                if self.ten_env:
                    if stream_sid:
                        self.ten_env.log_info(f"WebSocket connection closed for stream {stream_sid}")
                    else:
                        self.ten_env.log_info("WebSocket connection closed")
                # Clean up WebSocket reference
                for session_call_sid, session in self.active_call_sessions.items():
                    if session.get("websocket") == websocket:
                        session["websocket"] = None
                        break

        # Use configured WebSocket port
        websocket_port = self.config.twilio_server_media_ws_port
        server = await websockets.serve(websocket_handler, "0.0.0.0", websocket_port)

        if self.ten_env:
            self.ten_env.log_info(f"WebSocket server started on port {websocket_port}")

        await server.wait_closed()
