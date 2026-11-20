"""Observe HTTPX send and SSE record arrival before the Timbre decoder."""

import base64
import json
from functools import wraps

import httpx


class AudioRecord:
    """Classify captured lines at assertion time, independently per response."""

    def __init__(self):
        self.event = ""
        self.data = []

    def __call__(self, line):
        if line.startswith("event:"):
            self.event = line[6:].strip()
        elif line.startswith("data:"):
            self.data.append(line[5:])
        elif not line:
            event, data = self.event, self.data
            self.event, self.data = "", []
            if event == "chunk" and data:
                try:
                    return bool(
                        base64.b64decode(
                            json.loads("\n".join(data))["audio"], validate=True
                        )
                    )
                except (ValueError, KeyError, TypeError):
                    pass
        return False


class Hook:
    def install(self, patch, reference, config):
        original = httpx.AsyncClient.send

        @wraps(original)
        async def send(client, request, **kwargs):
            if str(request.url) != "https://api.vachana.ai/api/v1/tts/sse":
                return await original(client, request, **kwargs)
            request_id = request.extensions["ten_request_id"]
            reference.capture("text", request_id=request_id)
            response = await original(client, request, **kwargs)
            if response.is_success:
                lines = response.aiter_lines
                record = AudioRecord()

                async def observe_lines():
                    async for line in lines():
                        reference.capture("audio", line, record, request_id)
                        yield line

                response.aiter_lines = observe_lines
            return response

        patch.setattr(httpx.AsyncClient, "send", send)
