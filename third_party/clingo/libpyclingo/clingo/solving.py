"""
Functions and classes related to solving.

Examples
--------

The following example shows how to intercept models with a callback:

    >>> from clingo import Control
    >>>
    >>> ctl = Control(["0"])
    >>> ctl.add("base", [], "1 { a; b } 1.")
    >>> ctl.ground([("base", [])])
    >>> print(ctl.solve(on_model=print))
    Answer: a
    Answer: b
    SAT

The following example shows how to yield models:

    >>> from clingo import Control
    >>>
    >>> ctl = Control(["0"])
    >>> ctl.add("base", [], "1 { a; b } 1.")
    >>> ctl.ground([("base", [])])
    >>> with ctl.solve(yield_=True) as hnd:
    ...     for m in hnd:
    ...         print(m)
    ...     print(hnd.get())
    ...
    Answer: a
    Answer: b
    SAT

The following example shows how to solve asynchronously:

    >>> from clingo import Control
    >>>
    >>> ctl = Control(["0"])
    >>> ctl.add("base", [], "1 { a; b } 1.")
    >>> ctl.ground([("base", [])])
    >>> with ctl.solve(on_model=print, async_=True) as hnd:
    ...     # some computation here
    ...     hnd.wait()
    ...     print(hnd.get())
    ...
    Answer: a
    Answer: b
    SAT

This example shows how to solve both iteratively and asynchronously:

    >>> from clingo import Control
    >>>
    >>> ctl = Control(["0"])
    >>> ctl.add("base", [], "1 { a; b } 1.")
    >>> ctl.ground([("base", [])])
    >>> with ctl.solve(yield_=True, async_=True) as hnd:
    ...     while True:
    ...         hnd.resume()
    ...         # some computation here
    ...         _ = hnd.wait()
    ...         m = hnd.model()
    ...         if m is None:
    ...             print(hnd.get())
    ...             break
    ...         print(m)
    b
    a
    a b
    None
"""

from typing import ContextManager, Iterator, List, Optional, Sequence, Tuple, Union

from ._internal import _c_call, _c_call2, _ffi, _handle_error, _lib
from .util import Slice, SlicedSequence
from .core import OrderedEnum
from .symbol import Symbol
from .symbolic_atoms import SymbolicAtoms

__all__ = ["Model", "ModelType", "SolveControl", "SolveHandle", "SolveResult"]


class SolveResult:
    """
    Captures the result of a solve call.
    """

    def __init__(self, rep):
        self._rep = rep

    @property
    def exhausted(self) -> bool:
        """
        Determine if the search space was exhausted.
        """
        return (_lib.clingo_solve_result_exhausted & self._rep) != 0

    @property
    def interrupted(self) -> bool:
        """
        Determine if the search was interrupted.
        """
        return (_lib.clingo_solve_result_interrupted & self._rep) != 0

    @property
    def satisfiable(self) -> Optional[bool]:
        """
        `True` if the problem is satisfiable, `False` if the problem is
        unsatisfiable, or `None` if the satisfiablity is not known.
        """
        if (_lib.clingo_solve_result_satisfiable & self._rep) != 0:
            return True
        if (_lib.clingo_solve_result_unsatisfiable & self._rep) != 0:
            return False
        return None

    @property
    def unknown(self) -> bool:
        """
        Determine if the satisfiablity is not known.

        This is equivalent to satisfiable is None.
        """
        return self.satisfiable is None

    @property
    def unsatisfiable(self) -> Optional[bool]:
        """
        `True` if the problem is unsatisfiable, `False` if the problem is
        satisfiable, or `None` if the satisfiablity is not known.
        """
        if (_lib.clingo_solve_result_unsatisfiable & self._rep) != 0:
            return True
        if (_lib.clingo_solve_result_satisfiable & self._rep) != 0:
            return False
        return None

    def __str__(self):
        if self.satisfiable:
            return "SAT"
        if self.unsatisfiable:
            return "UNSAT"
        return "UNKNOWN"

    def __repr__(self):
        return f"SolveResult({self._rep})"


class SolveControl:
    """
    Object that allows for controlling a running search.
    """

    def __init__(self, rep):
        self._rep = rep

    def add_clause(self, literals: Sequence[Union[Tuple[Symbol, bool], int]]) -> None:
        """
        Add a clause that applies to the current solving step during the search.

        Parameters
        ----------
        literals
            List of literals either represented as pairs of symbolic atoms and
            Booleans or as program literals.

        Notes
        -----
        This function can only be called in a model callback or while iterating
        when using a `SolveHandle`.
        """
        atoms = self.symbolic_atoms
        p_lits = _ffi.new("clingo_literal_t[]", len(literals))
        for i, lit in enumerate(literals):
            if isinstance(lit, int):
                p_lits[i] = lit
            else:
                atom = atoms[lit[0]]
                if atom is not None:
                    slit = atom.literal
                else:
                    slit = -1
                p_lits[i] = slit if lit[1] else -slit

        _handle_error(
            _lib.clingo_solve_control_add_clause(self._rep, p_lits, len(literals))
        )

    def _invert(
        self, lit: Union[Tuple[Symbol, bool], int]
    ) -> Union[Tuple[Symbol, bool], int]:
        if isinstance(lit, int):
            return -lit
        return lit[0], not lit[1]

    def add_nogood(self, literals: Sequence[Union[Tuple[Symbol, bool], int]]) -> None:
        """
        Equivalent to `SolveControl.add_clause` with the literals inverted.
        """
        self.add_clause([self._invert(lit) for lit in literals])

    @property
    def symbolic_atoms(self) -> SymbolicAtoms:
        """
        `clingo.symbolic_atoms.SymbolicAtoms` object to inspect the symbolic atoms.
        """
        atoms = _c_call(
            "clingo_symbolic_atoms_t*",
            _lib.clingo_solve_control_symbolic_atoms,
            self._rep,
        )
        return SymbolicAtoms(atoms)


class ModelType(OrderedEnum):
    """
    Enumeration of the different types of models.
    """

    BraveConsequences = _lib.clingo_model_type_brave_consequences
    """
    The model stores the set of brave consequences.
    """
    CautiousConsequences = _lib.clingo_model_type_cautious_consequences
    """
    The model stores the set of cautious consequences.
    """
    StableModel = _lib.clingo_model_type_stable_model
    """
    The model captures a stable model.
    """


class _SymbolSequence(Sequence[Symbol]):
    """
    Helper class to efficiently store sequences of symbols.
    """

    def __init__(self, p_symbols):
        self._p_symbols = p_symbols

    def __len__(self):
        return len(self._p_symbols)

    def __getitem__(self, slc):
        if isinstance(slc, slice):
            return SlicedSequence(self, Slice(slc))
        if slc < 0:
            slc += len(self)
        if slc < 0 or slc >= len(self):
            raise IndexError("invalid index")
        return Symbol(self._p_symbols[slc])

    def __iter__(self):
        for i in range(len(self)):
            yield Symbol(self._p_symbols[i])

    def __str__(self):
        return f'[{", ".join(str(sym) for sym in self)}]'

    def __repr__(self):
        return f'[{", ".join(repr(sym) for sym in self)}]'


class Model:
    """
    Provides access to a model during a solve call and provides a
    `SolveContext` object to influence the running search.

    Notes
    -----
    The string representation of a model object is similar to the output of
    models by clingo using the default output.

    `Model` objects cannot be constructed from Python. Instead they are obained
    during solving (see `Control.solve`). Furthermore, the lifetime of a model
    object is limited to the scope of the callback it was passed to or until
    the search for the next model is started. They must not be stored for later
    use.
    """

    def __init__(self, rep):
        self._rep = rep

    def contains(self, atom: Symbol) -> bool:
        """
        Efficiently check if an atom is contained in the model.

        Parameters
        ----------
        atom
            The atom to lookup.

        Returns
        -------
        Whether the given atom is contained in the model.

        Notes
        -----
        The atom must be represented using a function symbol.
        """
        # pylint: disable=protected-access
        return _c_call("bool", _lib.clingo_model_contains, self._rep, atom._rep)

    def extend(self, symbols: Sequence[Symbol]) -> None:
        """
        Extend a model with the given symbols.

        Parameters
        ----------
        symbols
            The symbols to add to the model.

        Notes
        -----
        This only has an effect if there is an underlying clingo application,
        which will print the added symbols.
        """
        # pylint: disable=protected-access
        c_symbols = _ffi.new("clingo_symbol_t[]", len(symbols))
        for i, sym in enumerate(symbols):
            c_symbols[i] = sym._rep
        _handle_error(_lib.clingo_model_extend(self._rep, c_symbols, len(symbols)))

    def is_true(self, literal: int) -> bool:
        """
        Check if the given program literal is true.

        Parameters
        ----------
        literal
            The given program literal.

        Returns
        -------
        Whether the given program literal is true.
        """
        return _c_call("bool", _lib.clingo_model_is_true, self._rep, literal)

    def is_consequence(self, literal: int) -> Optional[bool]:
        """
        Check if the given program literal is a consequence.

        The function returns `True`, `False`, or `None` if the literal is a
        consequence, not a consequence, or it is not yet known whether it is a
        consequence, respectively.

        While enumerating cautious or brave consequences, there is partial
        information about which literals are consequences. The current state of
        a literal can be requested using this function. If this function is
        used during normal model enumeration, the function just returns whether
        a literal is true of false in the current model.

        Parameters
        ----------
        literal
            The given program literal.

        Returns
        -------
        Whether the given program literal is a consequence.
        """
        res = _c_call(
            "clingo_consequence_t", _lib.clingo_model_is_consequence, self._rep, literal
        )
        if res == _lib.clingo_consequence_true:
            return True
        if res == _lib.clingo_consequence_false:
            return False
        return None

    def symbols(
        self,
        atoms: bool = False,
        terms: bool = False,
        shown: bool = False,
        theory: bool = False,
        complement: bool = False,
    ) -> Sequence[Symbol]:
        """
        Return the list of atoms, terms, or CSP assignments in the model.

        Parameters
        ----------
        atoms
            Select all atoms in the model (independent of `#show` statements).
        terms
            Select all terms displayed with `#show` statements in the model.
        shown
            Select all atoms and terms as outputted by clingo.
        theory
            Select atoms added with `Model.extend`.
        complement
            Return the complement of the answer set w.r.t. to the atoms known
            to the grounder.

        Returns
        -------
        The selected symbols.

        Notes
        -----
        Atoms are represented using functions (`Symbol` objects), and CSP
        assignments are represented using functions with name `"$"` where the
        first argument is the name of the CSP variable and the second its
        value.
        """
        show = 0
        if atoms:
            show |= _lib.clingo_show_type_atoms
        if terms:
            show |= _lib.clingo_show_type_terms
        if shown:
            show |= _lib.clingo_show_type_shown
        if theory:
            show |= _lib.clingo_show_type_theory
        if complement:
            show |= _lib.clingo_show_type_complement

        size = _c_call("size_t", _lib.clingo_model_symbols_size, self._rep, show)

        p_symbols = _ffi.new("clingo_symbol_t[]", size)
        _handle_error(_lib.clingo_model_symbols(self._rep, show, p_symbols, size))

        return _SymbolSequence(p_symbols)

    def __str__(self):
        return " ".join(map(str, self.symbols(shown=True)))

    def __repr__(self):
        return f"Model({self._rep!r})"

    @property
    def context(self) -> SolveControl:
        """
        Object that allows for controlling the running search.
        """
        ctl = _c_call("clingo_solve_control_t*", _lib.clingo_model_context, self._rep)
        return SolveControl(ctl)

    @property
    def cost(self) -> List[int]:
        """
        Return the list of integer cost values of the model.

        The return values correspond to clasp's cost output.
        """
        size = _c_call("size_t", _lib.clingo_model_cost_size, self._rep)

        p_costs = _ffi.new("int64_t[]", size)
        _handle_error(_lib.clingo_model_cost(self._rep, p_costs, size))

        return list(p_costs)

    @property
    def priority(self) -> List[int]:
        """
        Return the priorities of the model's cost values.
        """
        size = _c_call("size_t", _lib.clingo_model_cost_size, self._rep)

        p_priorities = _ffi.new("clingo_weight_t[]", size)
        _handle_error(_lib.clingo_model_priority(self._rep, p_priorities, size))

        return list(p_priorities)

    @property
    def number(self) -> int:
        """
        The running number of the model.
        """
        return _c_call("uint64_t", _lib.clingo_model_number, self._rep)

    @property
    def optimality_proven(self) -> bool:
        """
        Whether the optimality of the model has been proven.
        """
        return _c_call("bool", _lib.clingo_model_optimality_proven, self._rep)

    @property
    def thread_id(self) -> int:
        """
        The id of the thread which found the model.
        """
        return _c_call("clingo_id_t", _lib.clingo_model_thread_id, self._rep)

    @property
    def type(self) -> ModelType:
        """
        The type of the model.
        """
        return ModelType(
            _c_call("clingo_model_type_t", _lib.clingo_model_type, self._rep)
        )


class SolveHandle(ContextManager["SolveHandle"]):
    """
    Handle for solve calls.

    They can be used to control solving, like, retrieving models or cancelling
    a search.

    See Also
    --------
    Control.solve

    Notes
    -----
    A `SolveHandle` is a context manager and must be used with Python's `with`
    statement.

    Blocking functions in this object release the GIL. They are not thread-safe
    though.
    """

    def __init__(self, rep, handler):
        self._rep = rep
        self._handler = handler

    def __iter__(self) -> Iterator[Model]:
        while True:
            self.resume()
            m = self.model()
            if m is None:
                break
            yield m

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        _handle_error(_lib.clingo_solve_handle_close(self._rep), self._handler)
        return False

    def cancel(self) -> None:
        """
        Cancel the running search.

        See Also
        --------
        clingo.control.Control.interrupt
        """
        _handle_error(_lib.clingo_solve_handle_cancel(self._rep), self._handler)

    def core(self) -> List[int]:
        """
        The subset of assumptions that made the problem unsatisfiable.
        """
        core, size = _c_call2(
            "clingo_literal_t*",
            "size_t",
            _lib.clingo_solve_handle_core,
            self._rep,
            handler=self._handler,
        )
        return [core[i] for i in range(size)]

    def get(self) -> SolveResult:
        """
        Get the result of a solve call.

        If the search is not completed yet, the function blocks until the
        result is ready.
        """
        res = _c_call(
            "clingo_solve_result_bitset_t",
            _lib.clingo_solve_handle_get,
            self._rep,
            handler=self._handler,
        )
        return SolveResult(res)

    def model(self) -> Optional[Model]:
        """
        Get the current model if there is any.
        """
        p_model = _ffi.new("clingo_model_t**")
        _handle_error(_lib.clingo_solve_handle_model(self._rep, p_model), self._handler)
        if p_model[0] == _ffi.NULL:
            return None
        return Model(p_model[0])

    def resume(self) -> None:
        """
        Discards the last model and starts searching for the next one.

        Notes
        -----
        If the search has been started asynchronously, this function starts the
        search in the background.
        """
        _handle_error(_lib.clingo_solve_handle_resume(self._rep), self._handler)

    def wait(self, timeout: Optional[float] = None) -> bool:
        """
        Wait for solve call to finish or the next result with an optional timeout.

        If a timeout is given, the behavior of the function changes depending
        on the sign of the timeout. If a postive timeout is given, the function
        blocks for the given amount time or until a result is ready. If the
        timeout is negative, the function will block until a result is ready,
        which also corresponds to the behavior of the function if no timeout is
        given. A timeout of zero can be used to poll if a result is ready.

        Parameters
        ----------
        timeout
            If a timeout is given, the function blocks for at most timeout seconds.

        Returns
        -------
        Indicates whether the solve call has finished or the next result is ready.
        """
        p_res = _ffi.new("bool*")
        _lib.clingo_solve_handle_wait(
            self._rep, -1 if timeout is None else timeout, p_res
        )
        return p_res[0]
