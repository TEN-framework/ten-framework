"""
Core functionality used throught the clingo package.
"""

from enum import Enum
from typing import Callable, Tuple

from ._internal import _cb_error_panic, _ffi, _lib, _to_str

__all__ = ["Logger", "MessageCode", "OrderedEnum", "TruthValue", "version"]


class OrderedEnum(Enum):
    """
    Enumeration of orderable elements.
    """

    def __ge__(self, other):
        if self.__class__ is other.__class__:
            return self.value >= other.value
        return NotImplemented

    def __gt__(self, other):
        if self.__class__ is other.__class__:
            return self.value > other.value
        return NotImplemented

    def __le__(self, other):
        if self.__class__ is other.__class__:
            return self.value <= other.value
        return NotImplemented

    def __lt__(self, other):
        if self.__class__ is other.__class__:
            return self.value < other.value
        return NotImplemented


def version() -> Tuple[int, int, int]:
    """
    Clingo's version as a tuple `(major, minor, revision)`.
    """
    p_major = _ffi.new("int*")
    p_minor = _ffi.new("int*")
    p_revision = _ffi.new("int*")
    _lib.clingo_version(p_major, p_minor, p_revision)
    return p_major[0], p_minor[0], p_revision[0]


class MessageCode(OrderedEnum):
    """
    Enumeration of messages codes.
    """

    AtomUndefined = _lib.clingo_warning_atom_undefined
    """
    Informs about an undefined atom in program.
    """
    FileIncluded = _lib.clingo_warning_file_included
    """
    Indicates that the same file was included multiple times.
    """
    GlobalVariable = _lib.clingo_warning_global_variable
    """
    Informs about a global variable in a tuple of an aggregate element.
    """
    OperationUndefined = _lib.clingo_warning_operation_undefined
    """
    Inform about an undefined arithmetic operation or unsupported weight of an
    aggregate.
    """
    Other = _lib.clingo_warning_other
    """
    Reports other kinds of messages.
    """
    RuntimeError = _lib.clingo_warning_runtime_error
    """
    To report multiple errors; a corresponding runtime error is raised later.
    """
    VariableUnbounded = _lib.clingo_warning_variable_unbounded
    """
    Informs about a CSP variable with an unbounded domain.
    """


Logger = Callable[[MessageCode, str], None]


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_logger_callback")
def _pyclingo_logger_callback(code, message, data):
    """
    Low-level logger callback.
    """
    handler = _ffi.from_handle(data)
    handler(MessageCode(code), _to_str(message))


class TruthValue(OrderedEnum):
    """
    Enumeration of the different truth values.
    """

    False_ = _lib.clingo_external_type_false
    """
    Represents truth value true.
    """
    Free = _lib.clingo_external_type_free
    """
    Represents absence of a truth value.
    """
    True_ = _lib.clingo_external_type_true
    """
    Represents truth value true.
    """
    Release = _lib.clingo_external_type_release
    """
    Indicates that an atom is to be released.
    """
