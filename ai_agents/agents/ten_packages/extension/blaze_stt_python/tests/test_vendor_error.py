#
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0.
# See the LICENSE file for more information.
#
import asyncio

from .mock import make_extension, outputs, vendor_event


def _error_for(event: dict) -> tuple[dict, object]:
    async def _run():
        extension, env = make_extension()
        await vendor_event(extension, event)
        (error,) = outputs(env, "error")
        return error, extension

    return asyncio.run(_run())


def test_auth_failure_is_fatal_with_vendor_info():
    error, extension = _error_for(
        {"type": "failed-authentication", "message": "authentication failed"}
    )
    assert error["code"] == -1000
    assert error["vendor_info"] == {
        "vendor": "blaze",
        "code": "401",
        "message": "authentication failed",
    }
    assert extension._session_fatal


def test_upstream_error_is_non_fatal():
    error, extension = _error_for(
        {
            "type": "error",
            "code": "upstream_error",
            "text": "Something went wrong. Please try again later.",
        }
    )
    assert error["code"] == 1000
    assert error["vendor_info"]["code"] == "upstream_error"
    assert not extension._session_fatal


def test_error_without_code_is_non_fatal():
    error, _ = _error_for({"type": "error", "text": "handler error"})
    assert error["code"] == 1000
    assert error["vendor_info"]["code"] == "unknown"
