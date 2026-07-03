#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from .turn_detector import TurnDetector, TurnDetectorDecision

import asyncio


class TENTurnDetectorExtension(AsyncExtension):
    def __init__(self, name: str):
        super().__init__(name)
        self.name = name
        self.config = None
        self.turn_detector = None
        self.eval_force_chat_task: asyncio.Task = None

        self.next_turn_id = (
            1  # start from 1, leave <=0 as reserved or special scenario
        )
        self.new_turn_started: bool = False
        self.cached_text: str = ""

    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        # read property to initialize configuration
        config_json, _ = await ten_env.get_property_to_json("")

        from .config import TENTurnDetectorConfig

        self.config = TENTurnDetectorConfig.model_validate_json(config_json)
        ten_env.log_info(f"config: {self.config.to_json()}")

    async def on_deinit(self, _ten_env: AsyncTenEnv) -> None:
        self.turn_detector = None

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        # create turn_detector
        self.turn_detector = TurnDetector(
            config=self.config,
            ten_env=ten_env,
        )

    async def on_stop(self, _ten_env: AsyncTenEnv) -> None:
        if self.turn_detector:
            await self.turn_detector.stop()
            self.turn_detector = None

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug(f"[on_cmd:{cmd_name}]")

        # TODO: handle cmd

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        if data_name != "text_data":
            return

        try:
            input_text, _ = data.get_property_string("text")
        except Exception as e:
            ten_env.log_warn(f"get_property_string(text) error: {e}")
            return

        is_final = False
        try:
            is_final, _ = data.get_property_bool("is_final")
        except Exception as _e:
            pass

        ten_env.log_debug(f"on_data text: {input_text} is_final: {is_final}")

        # cancel previous eval
        self.turn_detector.cancel_eval()

        out_text = self.cached_text + input_text
        if not self.new_turn_started:
            if is_final or len(out_text) >= 2:
                self.new_turn_started = True
                await self._flush(ten_env=ten_env)
            else:
                ten_env.log_debug("turn not started, skip")
                return

        # send out non-final text
        await self._send_non_turn_transcription(ten_env=ten_env, text=out_text)

        if not is_final:
            return

        self.cached_text += input_text  # catch final text

        # decide to chat or not
        await self._eval_decision(ten_env=ten_env)

    async def _flush(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("[send_cmd:flush]")
        new_cmd = Cmd.create("flush")
        await ten_env.send_cmd(new_cmd)
        ten_env.log_debug("cmd flush done")

    async def _send_non_turn_transcription(
        self, ten_env: AsyncTenEnv, text: str
    ) -> None:
        ten_env.log_debug(f"send non-final turn text {text}")

        out_data = Data.create("text_data")
        out_data.set_property_string("text", text)
        out_data.set_property_bool("is_final", False)
        await ten_env.send_data(data=out_data)

    async def _process_new_turn(
        self, ten_env: AsyncTenEnv, decision: TurnDetectorDecision
    ) -> None:
        text = self.cached_text
        self.cached_text = ""
        if not text:
            ten_env.log_warn("no cached transcription")
            text = ""  # create empty one

        self.next_turn_id += 1
        self.new_turn_started = False

        if decision == TurnDetectorDecision.Wait:
            ten_env.log_debug("end_of_turn by wait, no new turn to send")
            return

        ten_env.log_debug(f"end_of_turn, send new turn {text}")

        out_data = Data.create("text_data")
        out_data.set_property_string("text", text)
        out_data.set_property_bool("is_final", True)
        await ten_env.send_data(data=out_data)

    async def _eval_decision(self, ten_env: AsyncTenEnv) -> None:
        if not self.cached_text:
            ten_env.log_warn("no cached transcription, skip eval decision")
            return

        self.turn_detector.cancel_eval()

        # decide to finished or not
        decision = await self.turn_detector.eval(self.cached_text)
        if decision == TurnDetectorDecision.Finished:
            self._cancel_force_chat_task(ten_env=ten_env, recreate=False)
            await self._process_new_turn(
                ten_env=ten_env, decision=TurnDetectorDecision.Finished
            )
        elif decision == TurnDetectorDecision.Wait:
            self._cancel_force_chat_task(ten_env=ten_env, recreate=False)
            await self._process_new_turn(
                ten_env=ten_env, decision=TurnDetectorDecision.Wait
            )
        else:
            self._cancel_force_chat_task(ten_env=ten_env, recreate=True)

    async def _eval_force_chat(self, ten_env: AsyncTenEnv) -> None:
        if not self.config.force_chat_enabled():
            return  # disabled, do nothing

        await asyncio.sleep(self.config.force_threshold_ms / 1000)

        ten_env.log_info("force chat to process new turn")
        await self._process_new_turn(
            ten_env=ten_env, decision=TurnDetectorDecision.Finished
        )
        self.eval_force_chat_task = None  # finished

    def _cancel_force_chat_task(
        self, ten_env: AsyncTenEnv, recreate: bool = False
    ) -> None:
        if not self.config.force_chat_enabled():
            return

        if self.eval_force_chat_task:
            ten_env.log_info(
                f"cancel eval_force_chat task {self.eval_force_chat_task.get_name()}"
            )
            self.eval_force_chat_task.cancel()
            self.eval_force_chat_task = None

        if recreate:
            task = asyncio.create_task(self._eval_force_chat(ten_env=ten_env))
            ten_env.log_info(f"created eval_force_chat task {task.get_name()}")
            self.eval_force_chat_task = task
