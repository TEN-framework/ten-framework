"""
Internal functions and classes of the clingo module.
"""

from os import _exit
from traceback import print_exception
import os
import sys

try:
    _FLAGS = None
    # In the pip module the library also exports the clingo symbols, which
    # should be globally available for other libraries depending on clingo.
    if hasattr(sys, "setdlopenflags"):
        _FLAGS = sys.getdlopenflags()
        sys.setdlopenflags(os.RTLD_LAZY | os.RTLD_GLOBAL)
    try:
        # Note: imported first to correctly handle the embedded case
        from _clingo import ffi as _ffi, lib as _lib  # type: ignore # pylint: disable=no-name-in-module
    except ImportError:
        from ._clingo import ffi as _ffi, lib as _lib  # type: ignore # pylint: disable=no-name-in-module
finally:
    if _FLAGS is not None:
        sys.setdlopenflags(_FLAGS)


def _str(f_size, f_str, *args, handler=None):
    p_size = _ffi.new("size_t*")
    _handle_error(f_size(*args, p_size), handler)
    p_str = _ffi.new("char[]", p_size[0])
    _handle_error(f_str(*args, p_str, p_size[0]), handler)
    return _ffi.string(p_str).decode()


def _c_call(c_type, c_fun, *args, handler=None):
    """
    Helper to simplify calling C functions where the last parameter is a
    reference to the return value.
    """
    if isinstance(c_type, str):
        p_ret = _ffi.new(f"{c_type}*")
    else:
        p_ret = c_type
    _handle_error(c_fun(*args, p_ret), handler)
    return p_ret[0]


def _c_call2(c_type1, c_type2, c_fun, *args, handler=None):
    """
    Helper to simplify calling C functions where the last two parameters are a
    reference to the return value.
    """
    p_ret1 = _ffi.new(f"{c_type1}*")
    p_ret2 = _ffi.new(f"{c_type2}*")
    _handle_error(c_fun(*args, p_ret1, p_ret2), handler)
    return p_ret1[0], p_ret2[0]


def _to_str(c_str) -> str:
    return _ffi.string(c_str).decode()


def _handle_error(ret: bool, handler=None):
    if not ret:
        code = _lib.clingo_error_code()
        if (
            code == _lib.clingo_error_unknown
            and handler is not None
            and handler.error is not None
        ):
            raise handler.error[0](handler.error[1]).with_traceback(handler.error[2])
        msg = _ffi.string(_lib.clingo_error_message()).decode()
        if code == _lib.clingo_error_bad_alloc:
            raise MemoryError(msg)
        raise RuntimeError(msg)


def _cb_error_handler(param: str):
    def handler(exception, exc_value, traceback) -> bool:
        if traceback is not None:
            handler = _ffi.from_handle(traceback.tb_frame.f_locals[param])
            handler.error = (exception, exc_value, traceback)
            _lib.clingo_set_error(_lib.clingo_error_unknown, str(exc_value).encode())
        else:
            _lib.clingo_set_error(
                _lib.clingo_error_runtime, "error in callback".encode()
            )
        return False

    return handler


def _cb_error_panic(exception, exc_value, traceback):
    print_exception(exception, exc_value, traceback)
    sys.stderr.write("PANIC: exception in nothrow scope")
    _exit(1)


def _cb_error_print(exception, exc_value, traceback):
    print_exception(exception, exc_value, traceback)
    return False


class _Error:
    """
    Class to store an error in a unique location.
    """

    def __init__(self):
        self._error = None

    def clear(self):
        """
        Clears the last error set.
        """
        self._error = None

    @property
    def error(self):
        """
        Return the last error set.
        """
        # pylint: disable=protected-access,missing-function-docstring
        return self._error

    @error.setter
    def error(self, value):
        """
        Set an error if no error has been set before.

        This function is thread-safe.
        """
        # pylint: disable=protected-access
        self._error = value


class _CBData:
    """
    The class stores the data object that should be passed to a callback as
    well as provides the means to set an error while a callback is running.
    """

    def __init__(self, data, error):
        self.data = data
        self._error = error

    @property
    def error(self):
        """
        Get the last error in the underlying error object.
        """
        return self._error.error

    @error.setter
    def error(self, value):
        """
        Set error in the underlying error object.
        """
        self._error.error = value


def _overwritten(base, obj, function):
    return hasattr(obj, function) and (
        not hasattr(base, function)
        or getattr(base, function) is not getattr(obj.__class__, function, None)
    )
