#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import asyncio
from ten_runtime import (
    Cmd,
    StatusCode,
    AsyncExtensionTester,
    AsyncTenEnvTester,
)


class AsyncExtensionTesterBasic(AsyncExtensionTester):
    def __init__(self):
        super().__init__()
        self.ack_received = False

    def __del__(self):
        assert self.ack_received is True

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        handshake_cmd = Cmd.create("sync")

        ten_env.log_info("send sync cmd")
        result, error = await ten_env.send_cmd(
            handshake_cmd,
        )
        if error is not None:
            assert False, error

        assert result is not None

        statusCode = result.get_status_code()
        assert statusCode == StatusCode.OK

    async def on_stop(self, ten_env_tester: AsyncTenEnvTester) -> None:
        pass

    async def on_cmd(self, ten_env_tester: AsyncTenEnvTester, cmd: Cmd) -> None:
        if cmd.get_name() == "ack":
            ten_env_tester.stop_test()

            canceled = False

            try:
                await asyncio.sleep(10)
            except asyncio.CancelledError:
                # Because 'stop_test()' will close the extension tester, and
                # cancel all remaining tasks, so we can catch the
                # CancelledError.
                canceled = True
                print("on_cmd ack cancelled")
            finally:
                assert canceled is True
                self.ack_received = True
                print("on_cmd ack done")


def test_basic():
    tester = AsyncExtensionTesterBasic()
    tester.set_test_mode_single("default_extension_python")
    tester.run()


if __name__ == "__main__":
    test_basic()
