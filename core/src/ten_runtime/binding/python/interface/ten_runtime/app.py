#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from libten_runtime_python import (
    _App,  # pyright: ignore[reportPrivateUsage]
)

from .ten_env import TenEnv


class App(_App):
    def run(self, run_in_background: bool) -> None:
        if run_in_background:
            _App.run_internal(self, True)
        else:
            _App.run_internal(self, False)

    def close(self) -> None:
        _App.close_internal(self)

    def on_configure(self, ten_env: TenEnv) -> None:
        ten_env.on_configure_done()

    def on_init(self, ten_env: TenEnv) -> None:
        ten_env.on_init_done()

    def on_deinit(self, ten_env: TenEnv) -> None:
        ten_env.on_deinit_done()
