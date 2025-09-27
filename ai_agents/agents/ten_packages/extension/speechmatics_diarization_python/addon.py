#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

from ten_runtime import Addon, TenEnv, register_addon_as_extension


@register_addon_as_extension("speechmatics_diarization_python")
class SpeechmaticsDiarizationAddon(Addon):
    """Addon wrapper for the Speechmatics diarization extension."""

    def on_create_instance(self, ten: TenEnv, addon_name: str, context) -> None:
        from .extension import SpeechmaticsDiarizationExtension

        ten.on_create_instance_done(
            SpeechmaticsDiarizationExtension(addon_name), context
        )
