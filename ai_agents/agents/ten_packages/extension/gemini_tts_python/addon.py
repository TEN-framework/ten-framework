from ten_runtime import Addon, register_addon_as_extension, TenEnv
from .extension import GeminiTTSExtension


@register_addon_as_extension("gemini_tts_python")
class GeminiTTSExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.log_info(f"Creating Gemini TTS extension instance: {name}")
        ten_env.on_create_instance_done(GeminiTTSExtension(name), context)
