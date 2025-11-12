#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import time
import threading
from ten_runtime import (
    Addon,
    register_addon_as_extension,
    TenEnv,
    LogLevel,
)
from .extension import ServerExtension, ClientExtension


@register_addon_as_extension("default_extension_python")
class DefaultExtensionAddon(Addon):

    def thread_on_create_instance_done(
        self, ten_env: TenEnv, name: str, context
    ) -> None:
        # sleep 1 second to mock the time-consuming operation.
        time.sleep(1)

        if name == "server":
            ten_env.on_create_instance_done(ServerExtension(name), context)
        elif name == "client":
            ten_env.on_create_instance_done(ClientExtension(name), context)
        else:
            assert False

    def on_create_instance(
        self, ten_env: TenEnv, name: str, context: object
    ) -> None:
        ten_env.log(LogLevel.INFO, "on_create_instance" + name)

        # Create a new thread to call the on_create_instance_done function.
        threading.Thread(
            target=self.thread_on_create_instance_done,
            args=(ten_env, name, context),
        ).start()
