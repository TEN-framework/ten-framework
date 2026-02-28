#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
class Loc:
    app_uri: str | None
    graph_id: str | None
    extension_name: str | None

    def __init__(
        self,
        app_uri: str | None = None,
        graph_id: str | None = None,
        extension_name: str | None = None,
    ) -> None:
        self.app_uri = app_uri
        self.graph_id = graph_id
        self.extension_name = extension_name
