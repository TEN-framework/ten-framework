#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio
from unittest.mock import AsyncMock, MagicMock

from ..extension import BlazeSTTExtension


def _send_error(error_msg: str, error_code):
    async def _run():
        extension = BlazeSTTExtension("blaze_stt_python")
        extension.ten_env = MagicMock()
        extension.send_asr_error = AsyncMock()
        await extension.on_error(error_msg, error_code)
        return extension.send_asr_error.await_args.args

    return asyncio.run(_run())


def test_auth_failure_is_fatal_with_vendor_info():
    error, vendor_info = _send_error("authentication failed", 401)

    assert error.code == -1000
    assert vendor_info.vendor == "blaze"
    assert vendor_info.code == "401"
    assert vendor_info.message == "authentication failed"


def test_upstream_error_is_non_fatal():
    error, vendor_info = _send_error(
        "Something went wrong. Please try again later.", "upstream_error"
    )

    assert error.code == 1000
    assert vendor_info.code == "upstream_error"


def test_error_without_code_is_non_fatal():
    error, vendor_info = _send_error("handler error", None)

    assert error.code == 1000
    assert vendor_info.code == "unknown"
