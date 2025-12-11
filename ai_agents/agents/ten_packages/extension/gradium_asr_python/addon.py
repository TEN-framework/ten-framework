"""
Addon registration for Gradium ASR extension.
"""

from ten_runtime import Addon, register_addon_as_extension, TenEnv
from .extension import GradiumASRExtension


@register_addon_as_extension("gradium_asr_python")
class GradiumASRExtensionAddon(Addon):
    """Addon class for registering the Gradium ASR extension."""

    def on_create_instance(
        self, ten_env: TenEnv, addon_name: str, context
    ) -> None:
        """
        Create an instance of the Gradium ASR extension.

        Args:
            ten_env: TEN environment.
            addon_name: Name of the addon.
            context: Context for instance creation.
        """
        ten_env.log_info(
            f"Creating Gradium ASR extension instance: {addon_name}"
        )
        ten_env.on_create_instance_done(
            GradiumASRExtension(addon_name), context
        )
