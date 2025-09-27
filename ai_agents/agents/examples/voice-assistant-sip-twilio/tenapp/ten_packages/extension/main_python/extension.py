import asyncio
import json
import time
import base64
import os
import audioop
from datetime import datetime
from typing import Literal, Dict, Any, Optional
from .server import TwilioCallServer

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

        # Server management
        self.server_task: Optional[asyncio.Task] = None
        self.server_instance: Optional[Any] = None
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

        # Start the Twilio call server
        await self._start_server()

    async def _start_server(self):
        """Start the Twilio call server in the same process"""
        try:
            # Create server instance with config and ten_env
            self.server_instance = TwilioCallServer(self.config, self.ten_env)

            # Start the server as a background task using configured port
            self.server_task = asyncio.create_task(
                self.server_instance.start_server(port=self.config.twilio_server_port)
            )

            self.ten_env.log_info(f"Started Twilio call server on port {self.config.twilio_server_port} (HTTP + WebSocket)")

            # Wait a moment for the server to start
            await asyncio.sleep(1)

            self.ten_env.log_info("Twilio call server started successfully")

        except Exception as e:
            self.ten_env.log_error(f"Failed to start server: {str(e)}")
            raise

    async def _stop_server(self):
        """Stop the Twilio call server"""
        try:
            if self.server_task and not self.server_task.done():
                self.ten_env.log_info("Stopping Twilio call server")

                # Cancel the server task
                self.server_task.cancel()

                # Wait for task to complete
                try:
                    await asyncio.wait_for(self.server_task, timeout=5.0)
                except asyncio.TimeoutError:
                    self.ten_env.log_warning("Server task didn't stop gracefully")
                except asyncio.CancelledError:
                    pass  # Expected when cancelling

                self.ten_env.log_info("Twilio call server stopped successfully")

            # Cleanup server instance
            if self.server_instance:
                self.server_instance.cleanup()
                self.server_instance = None

            self.server_task = None

        except Exception as e:
            self.ten_env.log_error(f"Error stopping server: {str(e)}")

    async def _end_call_and_cleanup(self, call_sid: str):
        """End a call and cleanup resources"""
        try:
            # First, try to end the call via API
            if call_sid in self.active_call_sessions:
                self.ten_env.log_info(f"Ending call {call_sid} via API")

                # Make API call to end the call
                import aiohttp
                async with aiohttp.ClientSession() as session:
                    async with session.delete(f"http://localhost:{self.config.twilio_server_port}/api/call/{call_sid}") as response:
                        if response.status == 200:
                            self.ten_env.log_info(f"Call {call_sid} ended successfully")
                        else:
                            self.ten_env.log_warning(f"Failed to end call {call_sid} via API: {response.status}")

            # Cleanup local resources
            if call_sid in self.active_call_sessions:
                del self.active_call_sessions[call_sid]

            if call_sid in self.audio_dump_files:
                # Clean up audio dump file
                try:
                    os.remove(self.audio_dump_files[call_sid])
                    del self.audio_dump_files[call_sid]
                except Exception as e:
                    self.ten_env.log_warning(f"Failed to cleanup audio file for {call_sid}: {str(e)}")

        except Exception as e:
            self.ten_env.log_error(f"Error during call cleanup for {call_sid}: {str(e)}")

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

            # WebSocket server is now handled by the main server in server.py
            ten_env.log_info("WebSocket server is integrated with the main HTTP server")

    async def on_stop(self, ten_env: AsyncTenEnv):
        ten_env.log_info("[MainControlExtension] on_stop")
        self.stopped = True

        # End all active calls and cleanup
        for call_sid in list(self.active_call_sessions.keys()):
            await self._end_call_and_cleanup(call_sid)

        # Stop the server
        await self._stop_server()

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

        # Use the new cleanup method
        await self._end_call_and_cleanup(call_sid)
