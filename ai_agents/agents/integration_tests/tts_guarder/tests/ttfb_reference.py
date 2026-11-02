"""Independent first-text/first-audio observations at public provider boundaries."""

import os
from contextlib import contextmanager
from importlib import import_module
from threading import RLock

from .timing_hooks.transport import ObservedWebSockets


class TTFBReference:
    def __init__(self):
        self.lock = RLock()
        self.current_request_id = None
        self.sent = {}
        self.received = {}
        self.pending_events = []

    def begin_request(self, request_id):
        with self.lock:
            self.current_request_id = request_id

    def current_request(self):
        with self.lock:
            return self.current_request_id

    def text_sent(self, request_id, timestamp):
        with self.lock:
            if request_id == self.current_request_id:
                self.sent.setdefault(request_id, timestamp)

    def audio_received(self, request_id, timestamp):
        with self.lock:
            if request_id == self.current_request_id and request_id in self.sent:
                self.received.setdefault(request_id, timestamp)

    def defer_event(self, consumer, raw, timestamp):
        with self.lock:
            self.pending_events.append(
                (consumer, raw, timestamp, self.current_request_id)
            )

    def elapsed_ms(self, request_id):
        with self.lock:
            pending, self.pending_events = self.pending_events, []
        for consumer, raw, timestamp, captured_request_id in pending:
            consumer(raw, timestamp, captured_request_id)
        with self.lock:
            if request_id not in self.sent or request_id not in self.received:
                return None
            return (self.received[request_id] - self.sent[request_id]) / 1_000_000


HOOKS = {
    "elevenlabs_multiple_context_tts_python": (
        "elevenlabs",
        "ElevenLabsObserver",
        "elevenlabs_tts",
        "wss://api.elevenlabs.io",
    ),
    "bytedance_tts": (
        "bytedance",
        "ByteDanceObserver",
        "bytedance_tts",
        "wss://openspeech.bytedance.com/api/v1/tts/ws_binary",
    ),
    "bytedance_tts_duplex": (
        "bytedance_duplex",
        "ByteDanceDuplexObserver",
        "bytedance_tts",
        "wss://openspeech.bytedance.com/api/v3/tts/bidirection",
    ),
    "minimax_tts_websocket": (
        "minimax",
        "MiniMaxObserver",
        "minimax_tts",
        "wss://api.minimaxi.com/ws/v1/t2a_v2",
    ),
    "minimax_tts_websocket_duplex": (
        "minimax",
        "MiniMaxObserver",
        "minimax_tts",
        "wss://api.minimax.cn/ws/v1/t2a_v2_bidi",
    ),
    "soul_tts_python": (
        "soul",
        "SoulObserver",
        "soul_tts",
        "wss://qa-soul-llm.soulapp.cn/llm",
    ),
}


CONFIGS = {
    "bytedance_tts": ("TTSConfig", "uri"),
    "bytedance_tts_duplex": ("BytedanceTTSDuplexConfig", "api_url"),
    "elevenlabs_multiple_context_tts_python": ("ElevenLabsTTSConfig", "url"),
    "minimax_tts_websocket": ("MinimaxTTSWebsocketConfig", "url"),
    "minimax_tts_websocket_duplex": ("MinimaxTTSWebsocketDuplexConfig", "url"),
    "soul_tts_python": ("SoulTTSWebsocketConfig", "url"),
}


def resolve_environment(value):
    if isinstance(value, dict):
        return {key: resolve_environment(item) for key, item in value.items()}
    if isinstance(value, list):
        return [resolve_environment(item) for item in value]
    if isinstance(value, str) and value.startswith("${env:") and value.endswith("}"):
        variable, _, fallback = value[6:-1].partition("|")
        return os.environ.get(variable, fallback)
    return value


def effective_config(extension_name, config):
    """Reuse installed config normalization, never production timing/decoding."""
    module = import_module(f"ten_packages.extension.{extension_name}.config")
    class_name, field = CONFIGS[extension_name]
    normalized = getattr(module, class_name)(**resolve_environment(config))
    if extension_name == "elevenlabs_multiple_context_tts_python":
        normalized.apply_defaults()
    normalized.update_params()
    return module, normalized, getattr(normalized, field)


@contextmanager
def install_ttfb_reference(extension_name, config, monkeypatch):
    """Install a fresh protocol observer per connection; restore it on exit."""
    spec = HOOKS.get(extension_name)
    if spec is None:
        yield None
        return
    hook_module, hook_class, filename, default = spec
    config_module, normalized, url = effective_config(extension_name, config)
    if extension_name == "elevenlabs_multiple_context_tts_python":
        if config_module.uses_ttd_protocol(normalized.model_id):
            filename = "elevenlabs_ttd"
        url = url or normalized.params.get("base_url") or default
    if not url:
        raise ValueError(f"No provider URL for supported TTFB hook: {extension_name}")
    module = import_module(f"ten_packages.extension.{extension_name}.{filename}")
    hook = getattr(import_module(f".ttfb_hooks.{hook_module}", __package__), hook_class)
    reference = TTFBReference()
    with monkeypatch.context() as scoped:
        scoped.setattr(
            module,
            "websockets",
            ObservedWebSockets(
                module.websockets,
                url,
                None,
                match_path=extension_name != "elevenlabs_multiple_context_tts_python",
                observer_factory=lambda: hook(reference),
            ),
        )
        yield reference
