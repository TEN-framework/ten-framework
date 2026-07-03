"""
Functions and classes for symbol manipulation.

Examples
--------

    >>> from clingo.symbol import Function, Number, parse_term
    >>>
    >>> num = Number(42)
    >>> num.number
    42
    >>> fun = Function("f", [num])
    >>> fun.name
    'f'
    >>> [ str(arg) for arg in fun.arguments ]
    ['42']
    >>> parse_term(str(fun)) == fun
    True
    >>> clingo.parse_term('p(1+2)')
    p(3)
"""

from functools import total_ordering
from typing import Callable, List, Optional, Sequence

from ._internal import _c_call, _c_call2, _ffi, _lib, _str, _to_str
from .core import MessageCode, OrderedEnum

__all__ = [
    "Function",
    "Infimum",
    "Number",
    "String",
    "Supremum",
    "Symbol",
    "SymbolType",
    "Tuple_",
    "parse_term",
]


class SymbolType(OrderedEnum):
    """
    Enumeration of symbols types.
    """

    Function = _lib.clingo_symbol_type_function
    """
    A function symbol, e.g., `c`, `(1,"a")`, or `f(1,"a")`.
    """
    Infimum = _lib.clingo_symbol_type_infimum
    """
    The `#inf` symbol.
    """
    Number = _lib.clingo_symbol_type_number
    """
    A numeric symbol, e.g., `1`.
    """
    String = _lib.clingo_symbol_type_string
    """
    A string symbol, e.g., `"a"`.
    """
    Supremum = _lib.clingo_symbol_type_supremum
    """
    The `#sup` symbol
    """


@total_ordering
class Symbol:
    """
    Represents a gringo symbol.

    This includes numbers, strings, functions (including constants with
    `len(arguments) == 0` and tuples with `len(name) == 0`), `#inf` and `#sup`.

    Symbol objects implement Python's rich comparison operators and are ordered
    like in gringo. They can also be used as keys in dictionaries. Their string
    representation corresponds to their gringo representation.

    Notes
    -----
    Note that this class does not have a constructor. Instead there are the
    functions `Number`, `String`, `Tuple_`, and `Function` to construct symbol
    objects or the preconstructed symbols `Infimum` and `Supremum`.
    """

    __slots__ = ("_rep",)

    def __init__(self, rep):
        self._rep = rep

    def __str__(self) -> str:
        return _str(
            _lib.clingo_symbol_to_string_size, _lib.clingo_symbol_to_string, self._rep
        )

    def __repr__(self) -> str:
        if self.type == SymbolType.Infimum:
            return "Infimum"
        if self.type == SymbolType.Supremum:
            return "Supremum"
        if self.type == SymbolType.Number:
            return f"Number({self.number!r})"
        if self.type == SymbolType.String:
            return f"String({self.string!r})"
        assert self.type == SymbolType.Function
        return f"Function({self.name!r}, {self.arguments!r}, {self.positive!r})"

    def __hash__(self) -> int:
        return _lib.clingo_symbol_hash(self._rep)

    def __eq__(self, other: object) -> bool:
        if not isinstance(other, Symbol):
            return NotImplemented
        return _lib.clingo_symbol_is_equal_to(self._rep, other._rep)

    def __lt__(self, other: object) -> bool:
        if not isinstance(other, Symbol):
            return NotImplemented
        return _lib.clingo_symbol_is_less_than(self._rep, other._rep)

    def match(self, name: str, arity: int, positive: bool = True) -> bool:
        """
        Check if this is a function symbol with the given signature.

        Parameters
        ----------
        name
            The name of the function.

        arity
            The arity of the function.

        positive
            Whether to match positive or negative signatures.

        Returns
        -------
        Whether the function matches.
        """
        return (
            self.type == SymbolType.Function
            and self.positive == positive
            and self.name == name
            and len(self.arguments) == arity
        )

    @property
    def arguments(self) -> List["Symbol"]:
        """
        The arguments of a function.
        """
        args, size = _c_call2(
            "clingo_symbol_t*", "size_t", _lib.clingo_symbol_arguments, self._rep
        )
        return [Symbol(args[i]) for i in range(size)]

    @property
    def name(self) -> str:
        """
        The name of a function.
        """
        return _to_str(_c_call("char*", _lib.clingo_symbol_name, self._rep))

    @property
    def negative(self) -> bool:
        """
        The inverted sign of a function.
        """
        return _c_call("bool", _lib.clingo_symbol_is_negative, self._rep)

    @property
    def number(self) -> int:
        """
        The value of a number.
        """
        return _c_call("int", _lib.clingo_symbol_number, self._rep)

    @property
    def positive(self) -> bool:
        """
        The sign of a function.
        """
        return _c_call("bool", _lib.clingo_symbol_is_positive, self._rep)

    @property
    def string(self) -> str:
        """
        The value of a string.
        """
        return _to_str(_c_call("char*", _lib.clingo_symbol_string, self._rep))

    @property
    def type(self) -> SymbolType:
        """
        The type of the symbol.
        """
        return SymbolType(_lib.clingo_symbol_type(self._rep))


def Function(
    name: str, arguments: Sequence[Symbol] = [], positive: bool = True
) -> Symbol:
    """
    Construct a function symbol.

    This includes constants and tuples. Constants have an empty argument list
    and tuples have an empty name. Functions can represent classically negated
    atoms. Argument `positive` has to be set to false to represent such atoms.

    Parameters
    ----------
    name
        The name of the function (empty for tuples).
    arguments
        The arguments in form of a list of symbols.
    positive
        The sign of the function (tuples must not have signs).
    """
    # pylint: disable=protected-access,invalid-name,dangerous-default-value
    c_args = _ffi.new("clingo_symbol_t[]", len(arguments))
    for i, arg in enumerate(arguments):
        c_args[i] = arg._rep
    sym = _c_call(
        "clingo_symbol_t",
        _lib.clingo_symbol_create_function,
        name.encode(),
        c_args,
        len(arguments),
        positive,
    )
    return Symbol(sym)


def Number(number: int) -> Symbol:
    """
    Construct a numeric symbol given a number.

    Parameters
    ----------
    number
        The given number.
    """
    # pylint: disable=invalid-name
    p_rep = _ffi.new("clingo_symbol_t*")
    _lib.clingo_symbol_create_number(number, p_rep)
    return Symbol(p_rep[0])


def String(string: str) -> Symbol:
    """
    Construct a string symbol given a string.

    Parameters
    ----------
    string
        The given string.
    """
    # pylint: disable=invalid-name
    return Symbol(
        _c_call("clingo_symbol_t", _lib.clingo_symbol_create_string, string.encode())
    )


def Tuple_(arguments: Sequence[Symbol]) -> Symbol:
    """
    A shortcut for `Function("", arguments)`.

    Parameters
    ----------
    arguments
        The arguments in form of a list of symbols.

    See Also
    --------
    Function
    """
    # pylint: disable=invalid-name
    return Function("", arguments)


_p_infimum = _ffi.new("clingo_symbol_t*")
_p_supremum = _ffi.new("clingo_symbol_t*")
_lib.clingo_symbol_create_infimum(_p_infimum)
_lib.clingo_symbol_create_supremum(_p_supremum)

Infimum: Symbol = Symbol(_p_infimum[0])
Supremum: Symbol = Symbol(_p_supremum[0])


def parse_term(
    string: str,
    logger: Optional[Callable[[MessageCode, str], None]] = None,
    message_limit: int = 20,
) -> Symbol:
    """
    Parse the given string using gringo's term parser for ground terms.

    The function also evaluates arithmetic functions.

    Parameters
    ----------
    string
        The string to be parsed.
    logger
        Function to intercept messages normally printed to standard error.
    message_limit
        Maximum number of messages passed to the logger.
    """
    if logger is not None:
        # pylint: disable=protected-access
        c_handle = _ffi.new_handle(logger)
        c_cb = _lib.pyclingo_logger_callback
    else:
        c_handle = _ffi.NULL
        c_cb = _ffi.NULL
    return Symbol(
        _c_call(
            "clingo_symbol_t",
            _lib.clingo_parse_term,
            string.encode(),
            c_cb,
            c_handle,
            message_limit,
        )
    )
