#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json
import os
from typing_extensions import override
from ten_runtime import (
    Cmd,
    AsyncExtensionTester,
    AsyncTenEnvTester,
    AudioFrame,
)


class GraphTester1(AsyncExtensionTester):

    def __init__(self):
        super().__init__()
        self.received_audio_frame_count = 0

    async def on_start(self, ten_env: AsyncTenEnvTester) -> None:
        start_audio_file_reader_cmd = Cmd.create("get_status")
        result, err = await ten_env.send_cmd(start_audio_file_reader_cmd)
        if err is not None:
            ten_env.stop_test(err)
            return

        assert result is not None
        status, err = result.get_property_string("status")
        if err is not None:
            ten_env.stop_test(err)
            return

        ten_env.log_info(f"Status: {status}")

    @override
    async def on_audio_frame(
        self, ten_env: AsyncTenEnvTester, _audio_frame: AudioFrame
    ) -> None:
        ten_env.log_info(f"on_audio_frame: {_audio_frame}")
        self.received_audio_frame_count += 1
        if self.received_audio_frame_count == 10:
            ten_env.stop_test()

    async def on_stop(self, ten_env: AsyncTenEnvTester) -> None:
        ten_env.log_info("GraphTester1 on_stop")


def test_recv_cmd_after_stop_1():
    tester = GraphTester1()

    audio_file_path = os.path.join(
        os.path.dirname(__file__), "test_data", "test.pcm"
    )

    graph = {
        "nodes": [
            {
                "type": "extension",
                "name": "pcm_file_reader_python",
                "addon": "pcm_file_reader_python",
                "extension_group": "pcm_file_reader",
                "property": {
                    "audio_file_path": audio_file_path,
                    "sample_rate": 16000,
                },
            },
            {
                "type": "extension",
                "name": "simple_echo_python",
                "addon": "simple_echo_python",
                "extension_group": "simple_echo",
            },
            {
                "type": "extension",
                "name": "ten:test_extension",
                "addon": "ten:test_extension",
                "extension_group": "default_extension_group",
            },
        ],
        "connections": [
            {
                "extension": "ten:test_extension",
                "cmd": [
                    {
                        "name": "get_status",
                        "dest": [
                            {
                                "extension": "pcm_file_reader_python",
                            }
                        ],
                    }
                ],
                "audio_frame": [
                    {
                        "name": "pcm_frame",
                        "source": [
                            {
                                "extension": "simple_echo_python",
                            }
                        ],
                    }
                ],
            },
            {
                "extension": "pcm_file_reader_python",
                "audio_frame": [
                    {
                        "name": "pcm_frame",
                        "dest": [
                            {
                                "extension": "simple_echo_python",
                            }
                        ],
                    }
                ],
            },
        ],
    }

    tester.set_test_mode_graph(json.dumps(graph))
    tester.run()
