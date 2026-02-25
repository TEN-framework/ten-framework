from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("openclaw_gateway_tool_python")
class OpenclawGatewayToolAddon(Addon):
    def on_create_instance(
        self, ten_env: TenEnv, addon_name: str, context
    ) -> None:
        from .extension import OpenclawGatewayToolExtension

        ten_env.on_create_instance_done(
            OpenclawGatewayToolExtension(addon_name), context
        )
