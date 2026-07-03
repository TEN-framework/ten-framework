"""
This module defines the `Theory` class for using a [CFFI] based module with
clingo's Python package.

[CFFI]: https://cffi.readthedocs.io/en/latest/

Example
-------

```python
>>> from clingo import Control
>>> from clingo.ast import parse_string, ProgramBuilder
>>> from clingo.theory import Theory
>>> from clingcon._clingcon import lib, ffi
>>>
>>> prg = '&sum { x } >= 1. &sum { x } <= 3.'
>>>
>>> thy = Theory('clingcon', lib, ffi)
>>> ctl = Control(['0'])
>>> thy.register(ctl)
>>> with ProgramBuilder(ctl) as bld:
...     parse_string(prg, lambda ast: thy.rewrite_ast(ast, bld.add))
...
>>> ctl.ground([('base', [])])
>>> thy.prepare(ctl)
>>> with ctl.solve(yield_=True, on_model=thy.on_model) as hnd:
...     for mdl in hnd:
...         print([f'{key}={val}' for key, val in thy.assignment(mdl.thread_id)])
...
['x=1']
['x=2']
['x=3']
```

Notes
-----
The CFFI module must provide the following functions and data structures, where
`PREFIX` corresponds to the prefix string passed to the `Theory` constructor:

```c
enum PREFIX_value_type {
    PREFIX_value_type_int = 0,
    PREFIX_value_type_double = 1,
    PREFIX_value_type_symbol = 2
};
typedef int PREFIX_value_type_t;
typedef struct PREFIX_value {
    PREFIX_value_type_t type;
    union {
        int int_number;
        double double_number;
        clingo_symbol_t symbol;
    };
} PREFIX_value_t;
typedef bool (*PREFIX_ast_callback_t)(clingo_ast_t *ast, void *data);

bool PREFIX_create(PREFIX_theory_t **theory)
bool PREFIX_destroy(PREFIX_theory_t *theory)
bool PREFIX_version(PREFIX_theory_t **theory, int *major, int *minor, int *patch);
bool PREFIX_register(PREFIX_theory_t *theory, clingo_control_t* control)
bool PREFIX_rewrite_ast(PREFIX_theory_t *theory, clingo_ast_t *ast, PREFIX_ast_callback_t add, void *data);
bool PREFIX_prepare(PREFIX_theory_t *theory, clingo_control_t* control)
bool PREFIX_register_options(PREFIX_theory_t *theory, clingo_options_t* options)
bool PREFIX_validate_options(PREFIX_theory_t *theory)
bool PREFIX_on_model(PREFIX_theory_t *theory, clingo_model_t* model)
bool PREFIX_on_statistics(PREFIX_theory_t *theory, clingo_statistics_t* step, clingo_statistics_t* accu)
bool PREFIX_lookup_symbol(PREFIX_theory_t *theory, clingo_symbol_t symbol, size_t *index)
clingo_symbol_t PREFIX_get_symbol(PREFIX_theory_t *theory, size_t index)
void PREFIX_assignment_begin(PREFIX_theory_t *theory, uint32_t thread_id, size_t *index)
bool PREFIX_assignment_next(PREFIX_theory_t *theory, uint32_t thread_id, size_t *index)
bool PREFIX_assignment_has_value(PREFIX_theory_t *theory, uint32_t thread_id, size_t index)
void PREFIX_assignment_get_value(PREFIX_theory_t *theory, uint32_t thread_id, size_t index, PREFIX_value_t *value)
bool PREFIX_configure(PREFIX_theory_t *theory, char const *key, char const *value)
```
"""

from typing import Any, Callable, Iterator, Optional, Tuple, Union

from ._internal import _ffi, _handle_error, _lib
from .application import ApplicationOptions
from .ast import AST
from .control import Control
from .solving import Model
from .statistics import StatisticsMap
from .symbol import Symbol

__all__ = ["Theory"]

ValueType = Union[int, float, Symbol]


class _CBData:  # pylint: disable=too-few-public-methods
    """
    The class stores the data object that should be passed to a callback as
    well as provides the means to set an error while a callback is running.
    """

    def __init__(self, data):
        self.data = data
        self.error = None


class Theory:
    """
    Interface to call functions from a C-library extending clingo's C/Python
    library.

    The functions in here are designed to be used with a `Application`
    object but can also be used with a standalone `Control` object.
    """

    # pylint: disable=too-many-instance-attributes,line-too-long,protected-access

    def __init__(self, prefix: str, lib: Any, ffi: Any):
        """
        Loads a given library.

        Arguments
        ---------
        prefix
            Prefix of functions and data structures in the library.
        lib
            [A CFFI lib object](https://cffi.readthedocs.io/en/latest/using.html).
        ffi
            [A CFFI ffi object](https://cffi.readthedocs.io/en/latest/ref.html).
        """
        self._pre = prefix
        self._lib = lib
        self._ffi = ffi
        self._theory = self.__call1(self.__pre("theory_t*"), "create")
        self._rewrite = self.__define_rewrite()

    def __pre(self, name, extra=""):
        return f"{extra}{self._pre}_{name}"

    def __get(self, name, extra=""):
        return getattr(self._lib, self.__pre(name, extra))

    def __error_handler(self, exception, exc_value, traceback) -> bool:
        if traceback is not None:
            cb_data = self._ffi.from_handle(traceback.tb_frame.f_locals["data"])
            cb_data.error = (exception, exc_value, traceback)
            _lib.clingo_set_error(_lib.clingo_error_unknown, str(exc_value).encode())
        else:
            _lib.clingo_set_error(
                _lib.clingo_error_runtime, "error in callback".encode()
            )
        return False

    def __define_rewrite(self):
        @self._ffi.def_extern(
            name=self.__pre("rewrite", "py"), onerror=self.__error_handler
        )
        def rewrite(ast, data):
            add = self._ffi.from_handle(data).data
            ast = _ffi.cast("clingo_ast_t*", ast)
            _lib.clingo_ast_acquire(ast)
            add(AST(ast))
            return True

        return rewrite

    def __call(self, c_fun, *args):
        """
        Helper to simplify calling C functions without error handling.
        """
        return self.__get(c_fun)(*args)

    def __call0(self, c_fun, *args):
        """
        Helper to simplify calling C functions without a return value.
        """
        _handle_error(self.__call(c_fun, *args))

    def __call1(self, c_type, c_fun, *args):
        """
        Helper to simplify calling C functions where the last parameter is a
        reference to the return value.
        """
        if isinstance(c_type, str):
            p_ret = self._ffi.new(f"{c_type}*")
        else:
            p_ret = c_type
        _handle_error(self.__call(c_fun, *args, p_ret))
        return p_ret[0]

    def __del__(self):
        if self._theory is not None:
            self.__call0("destroy", self._theory)
            self._theory = None

    def version(self) -> Tuple[int, int, int]:
        """
        This function returns the version of the theory.

        Returns
        -------
        A 3-tuple of integers representing major and minor version as well as
        the patch level.
        """
        p_version = self._ffi.new("int[3]")
        self.__call("version", p_version, p_version + 1, p_version + 2)
        return p_version[0], p_version[1], p_version[2]

    def configure(self, key: str, value: str) -> None:
        """
        Allows for configuring a theory via key/value pairs similar to
        command line options.

        This function must be called before the theory is registered.

        Arguments
        ---------
        key
            The name of the option.
        value
            The value of the option.
        """
        self.__call0("configure", self._theory, key.encode(), value.encode())

    def register(self, control: Control) -> None:
        """
        Register the theory with the given control object.

        Arguments
        ---------
        control
            Target to register with.
        """
        self.__call0(
            "register", self._theory, self._ffi.cast("clingo_control_t*", control._rep)
        )

    def prepare(self, control: Control) -> None:
        """
        Prepare the theory.

        Must be called between ground and solve.

        Arguments
        ---------
        control
            The associated control object.
        """
        self.__call0(
            "prepare", self._theory, self._ffi.cast("clingo_control_t*", control._rep)
        )

    def rewrite_ast(self, stm: AST, add: Callable[[AST], None]) -> None:
        """
        Rewrite the given statement and call add on the rewritten version(s).

        Must be called for some theories that have to perform rewritings on the
        AST.

        Arguments
        ---------
        stm
            Statement to translate.
        add
            Callback for adding translated statements.
        """

        cb_data = _CBData(add)
        handle = self._ffi.new_handle(cb_data)
        self.__call0(
            "rewrite_ast",
            self._theory,
            self._ffi.cast("clingo_ast_t*", stm._rep),
            self.__get("rewrite", "py"),
            handle,
        )

    def register_options(self, options: ApplicationOptions) -> None:
        """
        Register the theory's options with the given application options
        object.

        Arguments
        ---------
        options
            Target to register with.
        """
        self.__call0(
            "register_options",
            self._theory,
            self._ffi.cast("clingo_options_t*", options._rep),
        )

    def validate_options(self) -> None:
        """
        Validate the options of the theory.
        """
        self.__call0("validate_options", self._theory)

    def on_model(self, model: Model) -> None:
        """
        Inform the theory that a model has been found.

        Arguments
        ---------
        model
            The current model.
        """
        self.__call0(
            "on_model", self._theory, self._ffi.cast("clingo_model_t*", model._rep)
        )

    def on_statistics(self, step: StatisticsMap, accu: StatisticsMap) -> None:
        """
        Add the theory's statistics to the given maps.

        Arguments
        ---------
        step: StatisticsMap
            Map for per step statistics.
        accu: StatisticsMap
            Map for accumulated statistics.
        """
        self.__call0(
            "on_statistics",
            self._theory,
            self._ffi.cast("clingo_statistics_t*", step._rep),
            self._ffi.cast("clingo_statistics_t*", accu._rep),
        )

    def lookup_symbol(self, symbol: Symbol) -> Optional[int]:
        """
        Get the integer index of a symbol assigned by the theory when a
        model is found.

        Using indices allows for efficent retrieval of values.

        Arguments
        ---------
        symbol
            The symbol to look up.

        Returns
        -------
        The index of the value if found.
        """
        c_index = self._ffi.new("size_t*")
        if self.__call("lookup_symbol", self._theory, symbol._rep, c_index):
            return c_index[0]
        return None

    def get_symbol(self, index: int) -> Symbol:
        """
        Get the symbol associated with an index.

        The index must be valid.

        Arguments
        ---------
        index
            Index to retreive.

        Returns
        -------
        The associated symbol.
        """
        return Symbol(
            _ffi.cast("clingo_symbol_t", self.__call("get_symbol", self._theory, index))
        )

    def has_value(self, thread_id: int, index: int) -> bool:
        """
        Check if the given symbol index has a value in the current model.

        Arguments
        ---------
        thread_id
            The index of the solving thread that found the model.
        index
            Index to retreive.

        Returns
        -------
        Whether the given index has a value.
        """
        return self.__call("assignment_has_value", self._theory, thread_id, index)

    def get_value(self, thread_id: int, index: int) -> ValueType:
        """
        Get the value of the symbol index in the current model.

        Arguments
        ---------
        thread_id
            The index of the solving thread that found the model.
        index
            Index to retreive.

        Returns
        -------
        The value of the index in form of an int, float, or Symbol.
        """
        c_value = self._ffi.new(self.__pre("value_t*"))
        self.__call("assignment_get_value", self._theory, thread_id, index, c_value)
        if c_value.type == 0:
            return c_value.int_number
        if c_value.type == 1:
            return c_value.double_number
        if c_value.type == 2:
            return Symbol(c_value.symbol)
        raise RuntimeError("must not happen")

    def assignment(self, thread_id: int) -> Iterator[Tuple[Symbol, ValueType]]:
        """
        Get all values symbol/value pairs assigned by the theory in the
        current model.

        Arguments
        ---------
        thread_id
            The index of the solving thread that found the model.

        Returns
        -------
        An iterator over symbol/value pairs.
        """
        c_index = self._ffi.new("size_t*")
        self.__call("assignment_begin", self._theory, thread_id, c_index)
        while self.__call("assignment_next", self._theory, thread_id, c_index):
            yield (self.get_symbol(c_index[0]), self.get_value(thread_id, c_index[0]))
