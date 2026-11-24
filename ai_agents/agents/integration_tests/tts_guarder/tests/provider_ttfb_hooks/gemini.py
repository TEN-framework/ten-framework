"""Observe Gemini Interactions request and first-audio boundaries."""

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

        def observe_audio(response, request_id):
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
            observe_audio(response, request_id)
            return response

        @wraps(original_async)
        async def observed_create_async(instance, *args, **kwargs):
            request_id = observe_text(kwargs)
            response = await original_async(instance, *args, **kwargs)
            observe_audio(response, request_id)
            return response

        patch.setattr(sync_class, "create", observed_create)
        patch.setattr(async_class, "create", observed_create_async)
        return reference
