#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import websockets
from ten_runtime import (
    Addon,
    AsyncExtension,
    register_addon_as_extension,
    TenEnv,
    Cmd,
    CmdResult,
    StatusCode,
    AsyncTenEnv,
)


class WebsocketServerExtension(AsyncExtension):
    def __init__(self, name: str) -> None:
        super().__init__(name)
        self.name = name

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        self.ten_env = ten_env

    async def echo(self, websocket, path):
        async for message in websocket:
            print(f"Received message: {message}")
            # Echo the message back to the client
            await websocket.send(f"Server received: {message}")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        self.server_port, err = await ten_env.get_property_int("server_port")
        if err is not None:
            ten_env.log_error(
                "Could not read 'server_port' from properties." + str(err)
            )
            self.server_port = 8002

        self.server = websockets.serve(self.echo, "localhost", self.server_port)
        self.ten_env.log_debug(
            f"Websocket server started on port {self.server_port}"
        )

        await self.server
        print("Websocket server started.")

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        ten_env.log_debug("on_cmd")

        # Not supported command.
        await ten_env.return_result(CmdResult.create(StatusCode.ERROR, cmd))

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")
        self.server.ws_server.close()


@register_addon_as_extension("websocket_server_python")
class DefaultExtensionAddon(Addon):
    def on_create_instance(self, ten_env: TenEnv, name: str, context) -> None:
        print("on_create_instance")
        ten_env.on_create_instance_done(WebsocketServerExtension(name), context)
