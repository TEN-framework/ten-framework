#
# This file is part of TEN Framework.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file in the root directory of this source tree.
#
from ten_runtime import Addon, TenEnv, register_addon_as_extension

from .extension import TimingFixtureExtension

TIMING_FIXTURE_ADDON = "guarder_timing_fixture"


@register_addon_as_extension(TIMING_FIXTURE_ADDON)
class TimingFixtureAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        ten_env.on_create_instance_done(TimingFixtureExtension(name), context)
