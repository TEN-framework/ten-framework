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

    async def on_stop(self, ten_env):
        await self._cancel_current_task()
        await super().on_stop(ten_env)

    def vendor(self):
        return "gnani"

    def synthesize_audio_sample_rate(self):
        return self.config.params.sample_rate
