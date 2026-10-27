"""Second-stage registry, independent of the pending first-stage guarder hooks."""

from contextlib import contextmanager
from importlib import import_module

from .reference import Reference

HOOKS = {
    "azure_tts": "azure",
    "gemini_tts_python": "gemini",
    "generic_tts_http_python": "generic_http",
    "openai_tts2_python": "openai",
    "mistral_tts_python": "mistral",
    "smallest_tts_python": "smallest",
    "ezai_tw_tts_python": "ezai",
    "sarvam_http_tts": "sarvam",
    "humeai_tts_python": "hume",
    "typecast_tts_python": "typecast",
    "google_tts_python": "google",
    "polly_tts": "polly",
    "oracle_tts_python": "oracle",
    "cosy_tts_python": "cosy",
    "murf_tts_python": "murf",
    "stepfun_tts_python": "stepfun",
}


@contextmanager
def install_hook(extension_name, config, monkeypatch):
    module = import_module(f".{HOOKS[extension_name]}", __package__)
    reference = Reference()
    with monkeypatch.context() as scoped:
        installed = module.Hook().install(scoped, reference, config)
        yield installed if installed is not None else reference
