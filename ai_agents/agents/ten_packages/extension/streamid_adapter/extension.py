#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import json
from ten_runtime import (
    AudioFrame,
    AsyncExtension,
    AsyncTenEnv,
)


class StreamIdAdapterExtension(AsyncExtension):
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

    async def on_audio_frame(
        self, ten_env: AsyncTenEnv, frame: AudioFrame
    ) -> None:
        # audio_frame_name = frame.get_name()
        # ten_env.log_info("on_audio_frame name {}".format(audio_frame_name))

        stream_id, _ = frame.get_property_int("stream_id")

        frame.set_property_from_json(
            "metadata",
            json.dumps(
                {
                    "session_id": f"{stream_id}",
                }
            ),
        )

        await ten_env.send_audio_frame(audio_frame=frame)
