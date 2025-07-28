#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
from dataclasses import dataclass
import json
from ten_runtime import (
    AsyncExtension,
    AsyncTenEnv,
    Cmd,
    StatusCode,
    CmdResult,
    Data,
)
from ten_ai_base.config import BaseConfig


@dataclass
class DataAdapterConfig(BaseConfig):
    pass


class DataAdapterExtension(AsyncExtension):
    async def on_init(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_init")

    async def on_start(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_start")

        # TODO: read properties, initialize resources

    async def on_stop(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_stop")

        # TODO: clean up resources

    async def on_deinit(self, ten_env: AsyncTenEnv) -> None:
        ten_env.log_debug("on_deinit")

    async def on_cmd(self, ten_env: AsyncTenEnv, cmd: Cmd) -> None:
        cmd_name = cmd.get_name()
        ten_env.log_debug("on_cmd name {}".format(cmd_name))

        # TODO: process cmd

        cmd_result = CmdResult.create(StatusCode.OK, cmd)
        await ten_env.return_result(cmd_result)

    async def on_data(self, ten_env: AsyncTenEnv, data: Data) -> None:
        data_name = data.get_name()
        ten_env.log_info("on_data name {}".format(data_name))

        if data_name == "asr_result":
            json_str, _ = data.get_property_to_json(None)

            json_data = json.loads(json_str)
            text = json_data.get("text", "")
            final = json_data.get("final", False)
            metadata = json_data.get("metadata", {})
            stream_id = int(metadata.get("session_id", "100"))

            ten_env.log_info(f"Received ASR result: {json_str}")

            output = Data.create("text_data")
            output.set_property_string("text", text)
            output.set_property_bool("is_final", final)
            output.set_property_bool("end_of_segment", final)
            output.set_property_int("stream_id", stream_id)

            await ten_env.send_data(output)
        if data_name == "text_data":
            json_str, _ = data.get_property_to_json(None)

            json_data = json.loads(json_str)
            text = json_data.get("text", "")
            final = json_data.get("final", False)
            metadata = json_data.get("metadata", {})
            stream_id = int(json_data.get("stream_id", "100"))

            ten_env.log_info(f"Received ASR result: {json_str}")

            output = Data.create("text_data")
            output.set_property_string("text", text)
            output.set_property_bool("is_final", final)
            output.set_property_bool("end_of_segment", final)
            output.set_property_int("stream_id", stream_id)

            await ten_env.send_data(output)
