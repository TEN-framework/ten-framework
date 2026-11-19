"""Observe gmicloud request submission and nonempty WAV arrival.

The GMI request-queue API is job-based: a submission runs to a completed
WAV, so the provider boundary is the client's HTTP exchange
(_synthesize_part), before any parsing or resampling. The interval
measured here intentionally includes job queueing and generation time;
that is the transport's stated TTFB limit.
"""

from functools import wraps
from importlib import import_module


class Hook:
    def install(self, patch, reference, config):
        module = import_module(
            "ten_packages.extension.gmicloud_tts2_python.client"
        )
        client = module.GmicloudTTSClient
        original = client._synthesize_part

        @wraps(original)
        async def observed(instance, text, request_id):
            if text:
                reference.capture("text", request_id=request_id)
            audio = await original(instance, text, request_id)
            if audio:
                reference.capture("audio", request_id=request_id)
            return audio

        patch.setattr(client, "_synthesize_part", observed)
