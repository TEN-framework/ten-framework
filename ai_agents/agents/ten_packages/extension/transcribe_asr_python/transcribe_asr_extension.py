from ten_ai_base.asr import AsyncASRBaseExtension
from ten_runtime import (
    Extension,
    TenEnv,
    Cmd,
    AudioFrame,
    StatusCode,
    CmdResult,
)

import asyncio
import threading

from .transcribe_wrapper import AsyncTranscribeWrapper, TranscribeConfig

PROPERTY_REGION = "region"  # Optional
PROPERTY_ACCESS_KEY = "access_key"  # Optional
PROPERTY_SECRET_KEY = "secret_key"  # Optional
PROPERTY_SAMPLE_RATE = "sample_rate"  # Optional
PROPERTY_LANG_CODE = "lang_code"  # Optional


class TranscribeAsrExtension(AsyncASRBaseExtension):
    def __init__(self, name: str):
        super().__init__(name)

        self.queue = asyncio.Queue(
            maxsize=3000
        )  # about 3000 * 10ms = 30s input
        self.transcribe = None

    async def _handle_reconnect(self):
        await asyncio.sleep(0.2)  # Adjust the sleep time as needed
        await self.stop_connection()
        await self.start_connection()

    def put_pcm_frame(self, ten: TenEnv, pcm_frame: AudioFrame) -> None:
        if self.stopped:
            return

        try:
            # Use a simpler synchronous approach with put_nowait
            if not self.loop.is_closed():
                if self.queue.qsize() < self.queue.maxsize:
                    self.loop.call_soon_threadsafe(
                        self.queue.put_nowait, pcm_frame
                    )
                else:
                    ten.log_error("Queue is full, dropping frame")
            else:
                ten.log_error("Event loop is closed, cannot process frame")
        except Exception as e:
            import traceback

            error_msg = f"Error putting frame in queue: {str(e)}\n{traceback.format_exc()}"
            ten.log_error(error_msg)

    def on_audio_frame(self, ten: TenEnv, frame: AudioFrame) -> None:
        self.put_pcm_frame(ten, pcm_frame=frame)

    def on_stop(self, ten: TenEnv) -> None:
        ten.log_info("TranscribeAsrExtension on_stop")

        # put an empty frame to stop transcribe_wrapper
        self.put_pcm_frame(ten, None)
        self.stopped = True
        self.thread.join()
        self.loop.stop()
        self.loop.close()

        ten.on_stop_done()

    def on_cmd(self, ten: TenEnv, cmd: Cmd) -> None:
        ten.log_info("TranscribeAsrExtension on_cmd")
        cmd_json = cmd.to_json()
        ten.log_info(f"TranscribeAsrExtension on_cmd json: {cmd_json}")

        cmdName = cmd.get_name()
        ten.log_info(f"got cmd {cmdName}")

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        cmd_result.set_property_string("detail", "success")
        ten.return_result(cmd_result)

    async def start_connection(self) -> None:
        self.ten_env.log_info("TranscribeAsrExtension on_start")

        transcribe_config = TranscribeConfig.default_config()

        for optional_param in [
            PROPERTY_REGION,
            PROPERTY_SAMPLE_RATE,
            PROPERTY_LANG_CODE,
            PROPERTY_ACCESS_KEY,
            PROPERTY_SECRET_KEY,
        ]:
            try:
                value, _ = ten.get_property_string(optional_param).strip()
                if value:
                    transcribe_config.__setattr__(optional_param, value)
            except Exception as err:
                ten.log_debug(
                    f"GetProperty optional {optional_param} failed, err: {err}. Using default value: {transcribe_config.__getattribute__(optional_param)}"
                )

        loop = asyncio.get_event_loop()
        self.transcribe = AsyncTranscribeWrapper(
            transcribe_config, self.queue, self.ten_env, loop
        )

        await asyncio.to_thread(self.transcribe.run)


    async def stop_connection(self) -> None:
        return await super().stop_connection()

    def is_connected(self) -> bool:
        return self.transcribe.is_connected()

    async def send_audio(self, frame: AudioFrame, session_id: str | None) -> bool:
        pass

    async def finalize(self, session_id: str | None) -> None:
        raise NotImplementedError(
            "Transcribe ASR does not support finalize operation yet."
        )
