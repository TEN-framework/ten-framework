#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from libten_runtime_python import (
    _Msg,  # pyright: ignore[reportPrivateUsage]
    _ten_py_msg_register_msg_type,  # pyright: ignore[reportPrivateUsage]
)
from .loc import Loc


class Msg(_Msg):
    def __init__(self, name: str):
        raise NotImplementedError(
            "Use [Cmd|Data|VideoFrame|AudioFrame].create instead."
        )

    def get_source(self) -> Loc:
        app_uri, graph_id, extension_name = _Msg.get_source_internal(self)
        return Loc(app_uri, graph_id, extension_name)


_ten_py_msg_register_msg_type(Msg)
