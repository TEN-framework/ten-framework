"""
Functions and classes to implement applications based on clingo.

Examples
--------
The following example reproduces the default clingo application:

    import sys
    from clingo.application import Application, clingo_main

    class ClingoApp(Application):
        def __init__(self, name):
            self.program_name = name

        def main(self, ctl, files):
            for f in files:
                ctl.load(f)
            if not files:
                ctl.load("-")
            ctl.ground([("base", [])])
            ctl.solve()

    clingo_main(ClingoApp(sys.argv[0]), sys.argv[1:])
"""

from typing import Any, Callable, List, Optional, Sequence
from abc import ABCMeta, abstractmethod
import sys

from ._internal import (
    _cb_error_print,
    _cb_error_panic,
    _ffi,
    _handle_error,
    _lib,
    _overwritten,
    _to_str,
)
from .core import MessageCode
from .solving import Model
from .control import Control

__all__ = ["Application", "ApplicationOptions", "Flag", "clingo_main"]


class Flag:
    """
    Helper object to parse command-line flags.

    Parameters
    ----------
    value
        The initial value of the flag.
    """

    def __init__(self, value: bool = False):
        self._flag = _ffi.new("bool*", value)

    def __bool__(self):
        return self.flag

    @property
    def flag(self) -> bool:
        """
        The value of the flag.
        """
        return self._flag[0]

    @flag.setter
    def flag(self, value: bool):
        self._flag[0] = value


class ApplicationOptions(metaclass=ABCMeta):
    """
    Object to add custom options to a clingo based application.
    """

    def __init__(self, rep, mem):
        self._rep = rep
        self._mem = mem

    def add(
        self,
        group: str,
        option: str,
        description: str,
        parser: Callable[[str], bool],
        multi: bool = False,
        argument: Optional[str] = None,
    ) -> None:
        """
        Add an option that is processed with a custom parser.

        Parameters
        ----------
        group
            Options are grouped into sections as given by this string.
        option
            Parameter option specifies the name(s) of the option. For example,
            `"ping,p"` adds the short option `-p` and its long form `--ping`. It is
            also possible to associate an option with a help level by adding `",@l"` to
            the option specification. Options with a level greater than zero are only
            shown if the argument to help is greater or equal to `l`.
        description
            The description of the option shown in the help output.
        parser
            An option parser is a function that takes a string as input and returns
            true or false depending on whether the option was parsed successively.
        multi
            Whether the option can appear multiple times on the command-line.
        argument
            Optional string to change the value name in the generated help.

        Notes
        -----
        The parser callback has to take care of storing the semantic value of
        the option somewhere.
        """
        # pylint: disable=protected-access
        c_data = _ffi.new_handle(parser)
        self._mem.append(c_data)

        _handle_error(
            _lib.clingo_options_add(
                self._rep,
                group.encode(),
                option.encode(),
                description.encode(),
                _lib.pyclingo_application_options_parse,
                c_data,
                multi,
                argument.encode() if argument is not None else _ffi.NULL,
            )
        )

    def add_flag(self, group: str, option: str, description: str, target: Flag) -> None:
        """
        Add an option that is a simple flag.

        This function is similar to `ApplicationOptions.add` but simpler because
        it only supports flags, which do not have values. Note that the target
        parameter must be of type Flag, which is set to true if the flag is passed on
        the command line.

        Parameters
        ----------
        group
            Options are grouped into sections as given by this string.
        option
            Same as for `ApplicationOptions.add`.
        description
            The description of the option shown in the help output.
        target
            The object that receives the value.
        """
        # pylint: disable=protected-access
        self._mem.append(target)
        _handle_error(
            _lib.clingo_options_add_flag(
                self._rep,
                group.encode(),
                option.encode(),
                description.encode(),
                target._flag,
            )
        )


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_options_parse")
def _pyclingo_application_options_parse(value, data):
    return _ffi.from_handle(data)(_to_str(value))


class Application(metaclass=ABCMeta):
    """
    Interface that has to be implemented to customize clingo.
    """

    program_name: str
    """
    Program name defaulting to `'clingo'` used in the help output.
    """
    version: str
    """
    Version string defaulting to clingo's version.
    """
    message_limit: int
    """
    Maximum number of messages defaulting to `20` passed to the logger.
    """

    @abstractmethod
    def main(self, control: Control, files: Sequence[str]) -> None:
        """
        Function to replace clingo's default main function.

        This function must be implemented.

        Parameters
        ----------
        control
            The main control object.
        files
            The files passed to clingo_main.
        """

    def register_options(self, options: ApplicationOptions) -> None:
        """
        Function to register custom options.

        Parameters
        ----------
        options
            Object to register additional options
        """

    def validate_options(self) -> bool:
        """
        Function to validate custom options.

        Returns
        -------
        This function should return false if option validation fails.
        """

    def print_model(self, model: Model, printer: Callable[[], None]) -> None:
        """
        Function to print additional information when the text output is used.

        To avoid entangled output, `sys.stdout.flush` is automatically called
        after this function.

        Parameters
        ----------
        model
            The current model
        printer
            The default printer as used in clingo.
        """

    def logger(self, code: MessageCode, message: str) -> None:
        """
        Function to intercept messages normally printed to standard error.

        By default, messages are printed to standard error.

        Parameters
        ----------
        code
            The message code.
        message
            The message string.

        Notes
        -----
        This function should not raise exceptions.
        """


def clingo_main(
    application: Application, arguments: Optional[Sequence[str]] = None
) -> int:
    """
    Runs the given application using clingo's default output and signal handling.

    The application can overwrite clingo's default behaviour by registering
    additional options and overriding its default main function.

    Parameters
    ----------
    application
        The application object.
    arguments
        The command line arguments excluding the program name.

        If omitted, then `sys.argv[1:]` is used.

    Returns
    -------
    The exit code of the application.

    Notes
    -----
    The main function of the `Application` interface has to be implemented. All
    other members are optional.
    """
    if arguments is None:
        arguments = sys.argv[1:]

    # pylint: disable=dangerous-default-value,protected-access,line-too-long
    c_application = _ffi.new(
        "clingo_application_t*",
        (
            _lib.pyclingo_application_program_name
            if _overwritten(Application, application, "program_name")
            else _ffi.NULL,
            _lib.pyclingo_application_version
            if _overwritten(Application, application, "version")
            else _ffi.NULL,
            _lib.pyclingo_application_message_limit
            if _overwritten(Application, application, "message_limit")
            else _ffi.NULL,
            _lib.pyclingo_application_main
            if _overwritten(Application, application, "main")
            else _ffi.NULL,
            _lib.pyclingo_application_logger
            if _overwritten(Application, application, "logger")
            else _ffi.NULL,
            _lib.pyclingo_application_print_model
            if _overwritten(Application, application, "print_model")
            else _ffi.NULL,
            _lib.pyclingo_application_register_options
            if _overwritten(Application, application, "register_options")
            else _ffi.NULL,
            _lib.pyclingo_application_validate_options
            if _overwritten(Application, application, "validate_options")
            else _ffi.NULL,
        ),
    )

    mem: List[Any] = []
    c_data = _ffi.new_handle((application, mem))

    return _lib.clingo_main(
        c_application,
        [_ffi.new("char[]", arg.encode()) for arg in arguments],
        len(arguments),
        c_data,
    )


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_program_name")
def _pyclingo_application_program_name(data):
    app, mem = _ffi.from_handle(data)
    mem.append(_ffi.new("char[]", app.program_name.encode()))
    return mem[-1]


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_version")
def _pyclingo_application_version(data):
    app, mem = _ffi.from_handle(data)
    mem.append(_ffi.new("char[]", app.version.encode()))
    return mem[-1]


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_message_limit")
def _pyclingo_application_message_limit(data):
    app = _ffi.from_handle(data)[0]
    return app.message_limit


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_logger")
def _pyclingo_application_logger(code, message, data):
    app = _ffi.from_handle(data)[0]
    return app.logger(MessageCode(code), _to_str(message))


@_ffi.def_extern(onerror=_cb_error_print, name="pyclingo_application_main")
def _pyclingo_application_main(control, files, size, data):
    app = _ffi.from_handle(data)[0]
    app.main(Control(control), [_to_str(files[i]) for i in range(size)])
    return True


@_ffi.def_extern(onerror=_cb_error_print, name="pyclingo_application_print_model")
def _pyclingo_application_print_model(model, printer, printer_data, data):
    def py_printer():
        _handle_error(printer(printer_data))

    app = _ffi.from_handle(data)[0]
    app.print_model(Model(model), py_printer)
    sys.stdout.flush()
    return True


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_register_options")
def _pyclingo_application_register_options(options, data):
    app, mem = _ffi.from_handle(data)
    app.register_options(ApplicationOptions(options, mem))
    return True


@_ffi.def_extern(onerror=_cb_error_panic, name="pyclingo_application_validate_options")
def _pyclingo_application_validate_options(data):
    app = _ffi.from_handle(data)[0]
    return app.validate_options()
