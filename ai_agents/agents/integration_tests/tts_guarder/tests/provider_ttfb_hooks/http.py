"""HTTPX request-body tracing and deferred protocol/audio decoding."""

import base64
import json
from importlib import import_module

import httpx


def has_audio(value, keys):
    if isinstance(value, dict):
        for key, item in value.items():
            if key in keys:
                chunks = item if isinstance(item, list) else [item]
                if any(
                    isinstance(chunk, str)
                    and chunk
                    and base64.b64decode(chunk, validate=True)
                    for chunk in chunks
                ):
                    return True
            if isinstance(item, (dict, list)) and has_audio(item, keys):
                return True
    elif isinstance(value, list):
        return any(has_audio(item, keys) for item in value)
    return False


class Decoder:
    def __init__(self, content_type, keys):
        self.content_type = content_type.lower()
        self.keys = keys
        self.pending = b""

    def feed(self, chunk):
        if not chunk:
            return False
        if "json" not in self.content_type and "event-stream" not in self.content_type:
            return True
        self.pending += chunk
        found = False
        if "event-stream" in self.content_type:
            self.pending = self.pending.replace(b"\r\n", b"\n")
            while b"\n\n" in self.pending:
                event, self.pending = self.pending.split(b"\n\n", 1)
                document = b"\n".join(
                    line[5:].lstrip()
                    for line in event.splitlines()
                    if line.startswith(b"data:")
                )
                if document and document != b"[DONE]":
                    found |= has_audio(json.loads(document), self.keys)
            return found
        # Accept complete JSON documents and newline-delimited SDK JSON streams.
        while b"\n" in self.pending:
            document, tail = self.pending.split(b"\n", 1)
            try:
                value = json.loads(document)
            except ValueError:
                break
            self.pending = tail
            found |= has_audio(value, self.keys)
        try:
            value = json.loads(self.pending)
        except ValueError:
            return found
        self.pending = b""
        return found or has_audio(value, self.keys)


class HttpxHook:
    module_name = ""
    binding = "AsyncClient"
    audio_keys = {"audio"}

    def install(self, patch, reference, config):
        module = import_module(self.module_name)
        original = (
            module.AsyncClient
            if self.binding == "AsyncClient"
            else module.httpx.AsyncClient
        )
        keys = self.audio_keys

        class Stream(httpx.AsyncByteStream):
            def __init__(self, stream, decoder):
                self.stream = stream
                self.decoder = decoder

            async def __aiter__(self):
                async for chunk in self.stream:
                    reference.capture("audio", chunk, self.decoder.feed)
                    yield chunk

            async def aclose(self):
                await self.stream.aclose()

        class Client(original):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)

                async def request_hook(request):
                    previous = request.extensions.get("trace")

                    async def trace(event, info):
                        if event in {
                            "http11.send_request_body.started",
                            "http2.send_request_body.started",
                        }:
                            reference.capture("text")
                        if previous is not None:
                            await previous(event, info)

                    request.extensions["trace"] = trace

                async def response_hook(response):
                    if response.status_code == 200:
                        response.stream = Stream(
                            response.stream,
                            Decoder(response.headers.get("content-type", ""), keys),
                        )

                self.event_hooks["request"].append(request_hook)
                self.event_hooks["response"].append(response_hook)

        if self.binding == "AsyncClient":
            patch.setattr(module, "AsyncClient", Client)
        else:
            # Replace only this extension's dependency binding, not global HTTPX.
            class Library:
                AsyncClient = Client

                def __getattr__(self, name):
                    return getattr(httpx, name)

            patch.setattr(module, "httpx", Library())
