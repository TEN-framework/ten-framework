"""Observe Gemini Interactions text sends and first streamed audio deltas."""

from functools import wraps
from importlib import import_module


def _input_has_text(input_items):
    if not isinstance(input_items, list):
        return False
    return any(
        isinstance(content, dict)
        and content.get("type") == "text"
        and bool(content.get("text"))
        for item in input_items
        if isinstance(item, dict)
        for content in item.get("content", [])
    )


def _audio_delta_data(event):
    if getattr(event, "event_type", None) != "step.delta":
        return None
    delta = getattr(event, "delta", None)
    if getattr(delta, "type", None) != "audio":
        return None
    data = getattr(delta, "data", None)
    return data if data else None


class _ObservedAsyncStream:
    def __init__(self, stream, reference, request_id):
        self.stream = stream
        self.reference = reference
        self.request_id = request_id

    def __aiter__(self):
        return self

    async def __anext__(self):
        event = await self.stream.__anext__()
        audio_data = _audio_delta_data(event)
        if audio_data:
            self.reference.capture(
                "audio",
                audio_data,
                request_id=self.request_id,
            )
        return event

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.close()

    async def close(self):
        close = getattr(self.stream, "aclose", None) or getattr(
            self.stream, "close", None
        )
        if close:
            return await close()
        return None


class _ObservedStream:
    def __init__(self, stream, reference, request_id):
        self.stream = iter(stream)
        self.reference = reference
        self.request_id = request_id

    def __iter__(self):
        return self

    def __next__(self):
        event = next(self.stream)
        audio_data = _audio_delta_data(event)
        if audio_data:
            self.reference.capture(
                "audio",
                audio_data,
                request_id=self.request_id,
            )
        return event

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def close(self):
        close = getattr(self.stream, "close", None)
        if close:
            return close()
        return None


class Hook:
    def install(self, patch, reference, config):
        module = import_module("google.genai._gaos.google_genai")
        sync_class = module.GeminiNextGenInteractions
        async_class = module.AsyncGeminiNextGenInteractions
        original_sync = sync_class.create
        original_async = async_class.create

        def observe_text(kwargs):
            request_id = reference.current_request()
            if _input_has_text(kwargs.get("input")):
                reference.capture("text", request_id=request_id)
            return request_id

        def observe_unary_audio(response, request_id):
            output_audio = getattr(response, "output_audio", None)
            audio_data = getattr(output_audio, "data", None)
            if audio_data:
                reference.capture(
                    "audio",
                    audio_data,
                    request_id=request_id,
                )

        @wraps(original_sync)
        def observed_create(instance, *args, **kwargs):
            request_id = observe_text(kwargs)
            response = original_sync(instance, *args, **kwargs)
            if kwargs.get("stream") and hasattr(response, "__iter__"):
                return _ObservedStream(response, reference, request_id)
            observe_unary_audio(response, request_id)
            return response

        @wraps(original_async)
        async def observed_create_async(instance, *args, **kwargs):
            request_id = observe_text(kwargs)
            response = await original_async(instance, *args, **kwargs)
            if kwargs.get("stream") and hasattr(response, "__aiter__"):
                return _ObservedAsyncStream(response, reference, request_id)
            observe_unary_audio(response, request_id)
            return response

        patch.setattr(sync_class, "create", observed_create)
        patch.setattr(async_class, "create", observed_create_async)
        return reference
