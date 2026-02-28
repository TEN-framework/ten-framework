#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from libten_runtime_python import (
    _Addon,  # pyright: ignore[reportPrivateUsage]
)

from .ten_env import TenEnv


class Addon(_Addon):
    def on_create_instance(
        self, ten_env: TenEnv, name: str, context: object
    ) -> None:
        return _Addon.on_create_instance_internal(
            self, ten_env._internal, name, context
        )
