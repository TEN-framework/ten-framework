'''
This module contains functions to add custom scripts, which can be embedded
into logic programs.

Examples
--------
The following example shows how to add a script that works the same way as
clingo's embedded Python script:
```python
>>> from clingo.script import Script, register_script
>>> from clingo.control import Control
>>>
>>> import __main__
>>>
>>> class MyPythonScript(Script):
...     def execute(self, location, code):
...         exec(code, __main__.__dict__, __main__.__dict__)
...     def call(self, location, name, arguments):
...         return getattr(__main__, name)(*arguments)
...     def callable(self, name):
...         return name in __main__.__dict__ and callable(__main__.__dict__[name])
...
>>> register_script('mypython', MyPythonScript())
>>>
>>> ctl = Control()
>>> ctl.add('base', [], """
... #script(mypython)
... from clingo.symbol import Number
... def func(a):
...     return Number(a.number + 1)
... #end.
... a(@func(1)).
... """)
>>>
>>> ctl.ground([('base',[])])
>>> ctl.solve(on_model=print)
a(2)
```
'''

from abc import ABCMeta, abstractmethod
from collections.abc import Iterable as IterableABC
from platform import python_version
from traceback import format_exception
from typing import Any, Iterable, List, Tuple, Union

from clingo._internal import _c_call, _ffi, _handle_error, _lib
from clingo.ast import Location, _py_location
from clingo.control import Control
from clingo.symbol import Symbol

try:
    import __main__  # type: ignore
except ImportError:
    # Note: pypy does not create a main module if embedded
    import sys
    import types

    sys.modules["__main__"] = types.ModuleType("__main__", "the main module")
    import __main__  # type: ignore

__all__ = ["Script", "enable_python", "register_script"]


def _cb_error_top_level(exception, exc_value, traceback):
    msg = "".join(format_exception(exception, exc_value, traceback))
    _lib.clingo_set_error(_lib.clingo_error_runtime, msg.encode())
    return False


class Script(metaclass=ABCMeta):
    """
    This interface can be implemented to embed custom scripting languages into
    logic programs.
    """

    @abstractmethod
    def execute(self, location: Location, code: str) -> None:
        """
        Execute the given source code.

        Parameters
        ----------
        location
            The location of the code.
        code
            The code to execute.
        """

    @abstractmethod
    def call(
        self, location: Location, name: str, arguments: Iterable[Symbol]
    ) -> Union[Iterable[Symbol], Symbol]:
        """
        Call the function with the given name and arguments.

        Parameters
        ----------
        location
            From where in the logic program the function was called.
        name
            The name of the function.
        arguments
            The arguments to the function.

        Returns
        -------
        The resulting pool of symbols.
        """

    @abstractmethod
    def callable(self, name: str) -> bool:
        """
        Check there is a function with the given name.

        Parameters
        ----------
        name
            The name of the function.

        Returns
        -------
        Whether the function is callable.
        """

    def main(self, control: Control) -> None:
        """
        Run the main function.

        This function exisits primarily for internal purposes and does not need
        to be implemented.

        Parameters
        ----------
        control
            Control object to pass to the main function.
        """


class _PythonScript(Script):
    def execute(self, location, code):
        exec(code, __main__.__dict__, __main__.__dict__)  # pylint: disable=exec-used

    def call(self, location, name, arguments):
        fun = getattr(__main__, name)
        return fun(*arguments)

    def callable(self, name):
        return name in __main__.__dict__ and callable(__main__.__dict__[name])

    def main(self, control):
        __main__.main(control)  # pylint: disable=c-extension-no-member


_PYTHON_SCRIPT = _PythonScript()
_GLOBAL_SCRIPTS: List[Tuple[Script, Any]] = []


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_script_execute")
def _pyclingo_script_execute(location, code, data):
    script: Script = _ffi.from_handle(data)
    script.execute(_py_location(location), _ffi.string(code).decode())
    return True


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_script_call")
def _pyclingo_script_call(
    location, name, arguments, size, symbol_callback, symbol_callback_data, data
):
    script: Script = _ffi.from_handle(data)
    symbol_callback = _ffi.cast("clingo_symbol_callback_t", symbol_callback)
    arguments = _ffi.cast("clingo_symbol_t*", arguments)
    py_name = _ffi.string(name).decode()
    py_args = [Symbol(arguments[i]) for i in range(size)]

    ret = script.call(_py_location(location), py_name, py_args)
    symbols = list(ret) if isinstance(ret, IterableABC) else [ret]

    c_symbols = _ffi.new("clingo_symbol_t[]", len(symbols))
    for i, sym in enumerate(symbols):
        c_symbols[i] = sym._rep  # pylint: disable=protected-access
    _handle_error(symbol_callback(c_symbols, len(symbols), symbol_callback_data))
    return True


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_script_callable")
def _pyclingo_script_callable(name, ret, data):
    script: Script = _ffi.from_handle(data)
    py_name = _ffi.string(name).decode()
    ret[0] = script.callable(py_name)
    return True


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_script_main")
def _pyclingo_script_main(ctl, data):
    script: Script = _ffi.from_handle(data)
    script.main(Control(_ffi.cast("clingo_control_t*", ctl)))
    return True


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_execute")
def _pyclingo_execute(location, code, data):
    assert data == _ffi.NULL
    return _pyclingo_script_execute(
        _ffi.cast("clingo_location_t*", location), code, _ffi.new_handle(_PYTHON_SCRIPT)
    )


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_call")
def _pyclingo_call(
    location, name, arguments, size, symbol_callback, symbol_callback_data, data
):
    assert data == _ffi.NULL
    return _pyclingo_script_call(
        _ffi.cast("clingo_location_t*", location),
        name,
        arguments,
        size,
        symbol_callback,
        symbol_callback_data,
        _ffi.new_handle(_PYTHON_SCRIPT),
    )


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_callable")
def _pyclingo_callable(name, ret, data):
    assert data == _ffi.NULL
    return _pyclingo_script_callable(name, ret, _ffi.new_handle(_PYTHON_SCRIPT))


@_ffi.def_extern(onerror=_cb_error_top_level, name="pyclingo_main")
def _pyclingo_main(ctl, data):
    assert data == _ffi.NULL
    return _pyclingo_script_main(
        _ffi.cast("clingo_control_t*", ctl), _ffi.new_handle(_PYTHON_SCRIPT)
    )


def register_script(name: str, script: Script, version: str = "1.0.0") -> None:
    """
    Registers a script language which can then be embedded into a logic
    program.

    Parameters
    ----------
    name
        The name of the script. This name can then be used in the script
        statement in a logic program.
    script
        The class to register.
    version
        The version of the script.
    """
    c_version = _c_call("char const*", _lib.clingo_add_string, version.encode())
    c_script = _ffi.new("clingo_script_t*")
    c_script[0].execute = _ffi.cast("void*", _lib.pyclingo_script_execute)
    c_script[0].call = _ffi.cast("void*", _lib.pyclingo_script_call)
    c_script[0].callable = _ffi.cast("void*", _lib.pyclingo_script_callable)
    c_script[0].main = _ffi.cast("void*", _lib.pyclingo_script_main)
    c_script[0].free = _ffi.NULL
    c_script[0].version = c_version
    data = _ffi.new_handle(script)
    _GLOBAL_SCRIPTS.append((script, data))
    _handle_error(_lib.clingo_register_script(name.encode(), c_script, data))


def enable_python() -> None:
    """
    This function can be called to enable evaluation of Python scripts in logic
    programs.

    By default evaluation is only enabled in the clingo executable but not in
    the Python module.
    """
    c_version = _c_call(
        "char const*", _lib.clingo_add_string, python_version().encode()
    )
    c_script = _ffi.new("clingo_script_t*")
    c_script[0].execute = _ffi.cast("void*", _lib.pyclingo_execute)
    c_script[0].call = _ffi.cast("void*", _lib.pyclingo_call)
    c_script[0].callable = _ffi.cast("void*", _lib.pyclingo_callable)
    c_script[0].main = _ffi.cast("void*", _lib.pyclingo_main)
    c_script[0].free = _ffi.NULL
    c_script[0].version = c_version
    _handle_error(_lib.clingo_register_script("python".encode(), c_script, _ffi.NULL))
