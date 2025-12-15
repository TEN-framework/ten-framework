#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import inspect

from libten_runtime_python import (
    _TenEnvTester,  # pyright: ignore[reportPrivateUsage]
)

from .log_level import LogLevel
from .error import TenError
from .value import Value
from .log_option import LogOption, DefaultLogOption


class TenEnvTesterBase:
    _internal: _TenEnvTester

    def __init__(self, internal_obj: _TenEnvTester) -> None:
        self._internal = internal_obj

    def __del__(self) -> None:
        pass

    def stop_test(self, error: TenError | None = None) -> TenError | None:
        return self._internal.stop_test(error)

    def log_debug(
        self,
        msg: str,
        category: str | None = None,
        fields: Value | None = None,
        option: LogOption = DefaultLogOption,
    ) -> TenError | None:
        return self._log_internal(LogLevel.DEBUG, msg, category, fields, option)

    def log_info(
        self,
        msg: str,
        category: str | None = None,
        fields: Value | None = None,
        option: LogOption = DefaultLogOption,
    ) -> TenError | None:
        return self._log_internal(LogLevel.INFO, msg, category, fields, option)

    def log_warn(
        self,
        msg: str,
        category: str | None = None,
        fields: Value | None = None,
        option: LogOption = DefaultLogOption,
    ) -> TenError | None:
        return self._log_internal(LogLevel.WARN, msg, category, fields, option)

    def log_error(
        self,
        msg: str,
        category: str | None = None,
        fields: Value | None = None,
        option: LogOption = DefaultLogOption,
    ) -> TenError | None:
        return self._log_internal(LogLevel.ERROR, msg, category, fields, option)

    def log(
        self,
        level: LogLevel,
        msg: str,
        category: str | None = None,
        fields: Value | None = None,
        option: LogOption = DefaultLogOption,
    ) -> TenError | None:
        return self._log_internal(level, msg, category, fields, option)

    def _log_internal(
        self,
        level: LogLevel,
        msg: str,
        category: str | None,
        fields: Value | None,
        option: LogOption,
    ) -> TenError | None:
        # Get the current frame.
        frame = inspect.currentframe()
        if frame is not None:
            try:
                # Skip the specified number of frames.
                for _ in range(option.skip):
                    if frame is not None:
                        frame = frame.f_back
                    else:
                        break

                if frame is not None:
                    # Extract information from the caller's frame.
                    file_name = frame.f_code.co_filename
                    func_name = frame.f_code.co_name
                    line_no = frame.f_lineno

                    return self._internal.log(
                        level,
                        func_name,
                        file_name,
                        line_no,
                        category,
                        msg,
                        option.sync,
                    )
            finally:
                # A defensive programming practice to ensure immediate cleanup
                # of potentially complex reference cycles.
                del frame

        # Fallback in case of failure to get caller information.
        return self._internal.log(
            level, None, None, 0, category, msg, option.sync
        )
