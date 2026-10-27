"""Queue WebSocket observations without decoding on the production I/O path."""

from importlib import import_module

from ..timing_hooks.transport import ObservedWebSockets


class WebSocketHook:
    module_name = ""
    default_url = ""

    def install(self, patch, reference, config):
        module = import_module(self.module_name)
        params = config.get("params") or {}
        url = (
            config.get("url")
            or config.get("base_url")
            or params.get("url")
            or params.get("base_url")
            or self.default_url
        )
        if url.startswith("${env:"):
            import os

            variable, _, default = url[6:-1].partition("|")
            url = os.environ.get(variable, default) or self.default_url
        hook = self

        class Observer:
            def observe_send(self, raw):
                request_id = reference.current_request()
                reference.capture(
                    "text",
                    raw,
                    lambda packet: hook.is_text(packet, request_id),
                    request_id=request_id,
                )

            def observe_receive(self, raw):
                request_id = reference.current_request()
                reference.capture(
                    "audio",
                    raw,
                    lambda packet: hook.is_audio(packet, request_id),
                    request_id=request_id,
                )

        patch.setattr(
            module,
            "websockets",
            ObservedWebSockets(module.websockets, url, Observer(), match_path=False),
        )
