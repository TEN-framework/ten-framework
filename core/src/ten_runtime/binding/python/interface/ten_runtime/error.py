#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
from libten_runtime_python import _TenError


class TenError(_TenError):
    def error_code(self) -> int:
        return _TenError.error_code(self)

    def error_message(self) -> str:
        return _TenError.error_message(self)
