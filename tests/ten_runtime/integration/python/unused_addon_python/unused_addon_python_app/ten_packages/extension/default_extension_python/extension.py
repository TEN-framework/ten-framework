#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from ten_runtime import Extension


class DefaultExtension(Extension):
    # The purpose of this case is to make sure the extension should not be
    # loaded if the extension is not declared in the graph.
    assert False
