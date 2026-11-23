#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import json

from ten_runtime import App, TenEnv
from ten_runtime.error import TenErrorCode


def test_runtime_stats_rejects_sync_python_writes():
    class StatsApp(App):
        def __init__(self):
            super().__init__()
            self.write_errors = []
            self.stats = None

        def on_init(self, ten_env: TenEnv) -> None:
            self.write_errors = [
                ten_env.set_property_from_json("ten:runtime_stats", "{}"),
                ten_env.set_property_int("ten:runtime_stats.metrics", 1),
            ]
            stats_json, error = ten_env.get_property_to_json(
                "ten:runtime_stats"
            )
            assert error is None
            self.stats = json.loads(stats_json)
            ten_env.on_init_done()
            self.close()

    app = StatsApp()
    app.run(False)

    for error in app.write_errors:
        assert error is not None
        assert error.error_code() == TenErrorCode.ErrorCodeInvalidArgument
    assert app.stats == {"metrics": {"resourceMetrics": []}}


if __name__ == "__main__":
    test_runtime_stats_rejects_sync_python_writes()
