#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from ten_ai_base.helper import AsyncQueue
from ten_runtime import AsyncTenEnv, Cmd


class LLMExec:
    """
    Context for LLM operations, including ASR and TTS.
    This class handles the interaction with the LLM, including processing commands and data.
    """

    def __init__(self, ten_env: AsyncTenEnv):
        self.ten_env = ten_env
        self.input_queue = AsyncQueue()
        self.stopped = False

    async def process_cmd(self, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        await self.cmd_events.put((cmd_name, cmd))