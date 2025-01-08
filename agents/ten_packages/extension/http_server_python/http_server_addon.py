from ten import (
    Addon,
    register_addon_as_extension,
    TenEnv,
)


@register_addon_as_extension("http_server_python")
class HTTPServerExtensionAddon(Addon):
    def on_create_instance(self, ten: TenEnv, addon_name: str, context):
        from .http_server_extension import HTTPServerExtension
        ten.log_info("on_create_instance")
        ten.on_create_instance_done(HTTPServerExtension(addon_name), context)
