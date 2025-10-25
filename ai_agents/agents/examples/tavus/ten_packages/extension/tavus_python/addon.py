from ten_runtime import Addon, register_addon_as_extension, TenEnv
from .extension import TavusExtension


@register_addon_as_extension("tavus_python")
class TavusExtensionAddon(Addon):
    """Addon for registering the Tavus extension."""

    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        """Create an instance of the Tavus extension."""
        ten_env.log_info(f"Creating TavusExtension instance: {name}")
        ten_env.on_create_instance_done(TavusExtension(name), context)
