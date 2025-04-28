#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    AsyncExtensionTester,
    AsyncTenEnvTester,
    Cmd,
)


class OllamaExtensionTester(AsyncExtensionTester):
    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        await self.test_ask_cmd(ten_env)

    async def test_ask_cmd(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_debug("send ask cmd")

        ask_cmd = Cmd.create("ask")
        ask_cmd.set_property_string("question", "hi")
        async for result, error in ten_env.send_cmd_ex(ask_cmd):
            if error is not None:
                ten_env.log_error(f"error: {error}")
            elif result is not None:
                if result.is_completed():
                    ten_env.stop_test()
                    break
                else:
                    response, _ = result.get_property_string("response")
                    ten_env.log_info(f"result: {response}")


def test_basic():
    tester = OllamaExtensionTester()
    tester.set_test_mode_single("ollama_python")
    tester.run()
