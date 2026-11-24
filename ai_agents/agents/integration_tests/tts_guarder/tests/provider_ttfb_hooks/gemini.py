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
        client_class = module.GeminiNextGenInteractions
        original = client_class.create

        @wraps(original)
        def observed_create(instance, *args, **kwargs):
            request_id = reference.current_request()
            if _input_has_text(kwargs.get("input")):
                reference.capture("text", request_id=request_id)
            response = original(instance, *args, **kwargs)
            output_audio = getattr(response, "output_audio", None)
            audio_data = getattr(output_audio, "data", None)
            if audio_data:
                reference.capture(
                    "audio",
                    audio_data,
                    request_id=request_id,
                )
            return response

        patch.setattr(client_class, "create", observed_create)
        return reference
