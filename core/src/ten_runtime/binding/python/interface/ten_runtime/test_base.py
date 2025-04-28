#
# Copyright © 2025 Agora
# This file is part of TEN Framework, an open source project.
# Licensed under the Apache License, Version 2.0, with certain conditions.
# Refer to the "LICENSE" file in the root directory for more information.
#
import inspect
from typing import Optional
from libten_runtime_python import _TenEnvTester
from .log_level import LogLevel
from .error import TenError


class TenEnvTesterBase:

    def __init__(self, internal_obj: _TenEnvTester) -> None:
        self._internal = internal_obj

    def __del__(self) -> None:
        pass

    def log_verbose(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.VERBOSE, msg, 2)

    def log_debug(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.DEBUG, msg, 2)

    def log_info(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.INFO, msg, 2)

    def log_warn(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.WARN, msg, 2)

    def log_error(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.ERROR, msg, 2)

    def log_fatal(self, msg: str) -> Optional[TenError]:
        return self._log_internal(LogLevel.FATAL, msg, 2)

    def _log_internal(
        self, level: LogLevel, msg: str, skip: int
    ) -> Optional[TenError]:
        # Get the current frame.
        frame = inspect.currentframe()
        if frame is not None:
            try:
                # Skip the specified number of frames.
                for _ in range(skip):
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
                        level, func_name, file_name, line_no, msg
                    )
            finally:
                # A defensive programming practice to ensure immediate cleanup
                # of potentially complex reference cycles.
                del frame

        # Fallback in case of failure to get caller information.
        return self._internal.log(level, None, None, 0, msg)
