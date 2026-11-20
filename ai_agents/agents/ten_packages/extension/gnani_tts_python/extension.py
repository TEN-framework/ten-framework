import asyncio

from ten_ai_base.tts2_http import AsyncTTS2HttpExtension

from .config import GnaniTTSConfig
from .gnani_tts import GnaniTTSClient


class GnaniTTSExtension(AsyncTTS2HttpExtension):
    async def create_config(self, config_json_str):
        return GnaniTTSConfig.model_validate_json(config_json_str)

    async def create_client(self, config, ten_env):
        config.validate()
        return GnaniTTSClient(config.params)

    async def request_tts(self, text_input):
        # TEN flush cancels current_task. Keep the HTTP read and any
        # rate-limit backoff inside this task, separate from the input loop.
        task = asyncio.create_task(super().request_tts(text_input))
        self.current_task = task
        try:
            await task
        finally:
            if self.current_task is task:
                self.current_task = None

    async def _cancel_current_task(self):
        task = self.current_task
        await super()._cancel_current_task()
        if task is not None:
            await asyncio.gather(task, return_exceptions=True)

    async def cancel_tts(self):
        await super().cancel_tts()
        # A flush before audio_start has no audio_end to release the dump writer.
        recorder = self.recorder_map.pop(self.current_request_id, None)
        if recorder is not None:
            await recorder.flush()

    async def on_stop(self, ten_env):
        await self._cancel_current_task()
        await super().on_stop(ten_env)

    async def send_tts_ttfb_metrics(
        self, request_id, ttfb_ms, turn_id=-1, extra_metadata=None
    ):
        await super().send_tts_ttfb_metrics(
            request_id, self.client.first_audio_ms, turn_id, extra_metadata
        )

    def vendor(self):
        return "gnani"

    def synthesize_audio_sample_rate(self):
        return self.config.params.sample_rate
