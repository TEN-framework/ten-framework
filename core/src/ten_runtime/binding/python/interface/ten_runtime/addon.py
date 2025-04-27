#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from libten_runtime_python import _Addon
from .ten_env import TenEnv


class Addon(_Addon):
    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_init")
        ten_env.on_init_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.log_debug("on_deinit")
        ten_env.on_deinit_done()
