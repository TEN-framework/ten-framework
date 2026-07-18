#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#

import pytest

from ten_ai_base.message import ModuleErrorCode

from ..extension import SonioxASRErrorFilter


@pytest.mark.parametrize(
    "error_code,error_message,expected",
    [
        (400, "No audio received", ModuleErrorCode.NON_FATAL_ERROR.value),
        (400, "Audio is too long", ModuleErrorCode.NON_FATAL_ERROR.value),
        (
            400,
            "No audio received and extra",
            ModuleErrorCode.NON_FATAL_ERROR.value,
        ),
        (
            400,
            "Prefix Audio is too long suffix",
            ModuleErrorCode.NON_FATAL_ERROR.value,
        ),
        (400, "Other bad request", ModuleErrorCode.FATAL_ERROR.value),
        (400, "Invalid parameter", ModuleErrorCode.FATAL_ERROR.value),
        (401, "Unauthorized", ModuleErrorCode.FATAL_ERROR.value),
        (401, "any message", ModuleErrorCode.FATAL_ERROR.value),
        (402, "Payment required", ModuleErrorCode.FATAL_ERROR.value),
        (402, "any message", ModuleErrorCode.FATAL_ERROR.value),
        (500, "Internal server error", ModuleErrorCode.NON_FATAL_ERROR.value),
        (-1, "Connection exception", ModuleErrorCode.NON_FATAL_ERROR.value),
    ],
)
def test_error_filter_get_module_error_code(
    error_code: int, error_message: str, expected: int
) -> None:
    assert (
        SonioxASRErrorFilter.get_module_error_code(error_code, error_message)
        == expected
    )
