"""
Functions and classes to observe or add ground statements.

Examples
--------
The following example shows how to add a fact to a program via the backend and
observe the corresponding rule passed to the backend:

    >>> from clingo.symbol import Function
    >>> from clingo.control import Control
    >>>
    >>> class Observer:
    ...     def rule(self, choice, head, body):
    ...         print("rule:", choice, head, body)
    ...
    >>> ctl = Control()
    >>> ctl.register_observer(Observer())
    >>>
    >>> sym_a = Function("a")
    >>> with ctl.backend() as backend:
    ...     atm_a = backend.add_atom(sym_a)
    ...     backend.add_rule([atm_a])
    ...
    rule: False [1] []
    >>> ctl.symbolic_atoms[sym_a].is_fact
    True
    >>>
    >>> print(ctl.solve(on_model=print))
    a
    SAT
"""

from typing import ContextManager, Sequence, Optional, Tuple
from abc import ABCMeta

from ._internal import _c_call, _cb_error_handler, _ffi, _handle_error, _lib, _to_str
from .core import OrderedEnum, TruthValue
from .symbol import Symbol

__all__ = ["Backend", "HeuristicType", "Observer", "TheorySequenceType"]


class TheorySequenceType(OrderedEnum):
    """
    Enumeration of the different heuristic types.
    """

    Tuple = _lib.clingo_theory_sequence_type_tuple
    """
    Type for tuples.
    """
    List = _lib.clingo_theory_sequence_type_list
    """
    Type for lists.
    """
    Set = _lib.clingo_theory_sequence_type_set
    """
    Type fo sets.
    """


class HeuristicType(OrderedEnum):
    """
    Enumeration of the different heuristic types.
    """

    Factor = _lib.clingo_heuristic_type_factor
    """
    Heuristic modification to set the decaying factor of an atom.
    """
    False_ = _lib.clingo_heuristic_type_false
    """
    Heuristic modification to make an atom false.
    """
    Init = _lib.clingo_heuristic_type_init
    """
    Heuristic modification to set the inital score of an atom.
    """
    Level = _lib.clingo_heuristic_type_level
    """
    Heuristic modification to set the level of an atom.
    """
    Sign = _lib.clingo_heuristic_type_sign
    """
    Heuristic modification to set the sign of an atom.
    """
    True_ = _lib.clingo_heuristic_type_true
    """
    Heuristic modification to make an atom true.
    """


class Observer(metaclass=ABCMeta):
    """
    Interface that has to be implemented to inspect rules produced during
    grounding.

    See Also
    --------
    clingo.control.Control.register_observer

    Notes
    -----
    Not all functions the `Observer` interface have to be implemented and can
    be omitted if not needed.
    """

    def init_program(self, incremental: bool) -> None:
        """
        Called once in the beginning.

        Parameters
        ----------
        incremental
            Whether the program is incremental. If the incremental flag is
            true, there can be multiple calls to `clingo.control.Control.solve`.
        """

    def begin_step(self) -> None:
        """
        Marks the beginning of a block of directives passed to the solver.
        """

    def rule(self, choice: bool, head: Sequence[int], body: Sequence[int]) -> None:
        """
        Observe rules passed to the solver.

        Parameters
        ----------
        choice
            Determines if the head is a choice or a disjunction.
        head
            List of program atoms forming the rule head.
        body
            List of program literals forming the rule body.
        """

    def weight_rule(
        self,
        choice: bool,
        head: Sequence[int],
        lower_bound: int,
        body: Sequence[Tuple[int, int]],
    ) -> None:
        """
        Observe rules with one weight constraint in the body passed to the
        solver.

        Parameters
        ----------
        choice
            Determines if the head is a choice or a disjunction.
        head
            List of program atoms forming the head of the rule.
        lower_bound
            The lower bound of the weight constraint in the rule body.
        body
            List of weighted literals (pairs of literal and weight) forming the
            elements of the weight constraint.
        """

    def minimize(self, priority: int, literals: Sequence[Tuple[int, int]]) -> None:
        """
        Observe minimize directives (or weak constraints) passed to the
        solver.

        Parameters
        ----------
        priority
            The priority of the directive.
        literals
            List of weighted literals whose sum to minimize (pairs of literal
            and weight).
        """

    def project(self, atoms: Sequence[int]) -> None:
        """
        Observe projection directives passed to the solver.

        Parameters
        ----------
        atoms
            The program atoms to project on.
        """

    def output_atom(self, symbol: Symbol, atom: int) -> None:
        """
        Observe shown atoms passed to the solver.  Facts do not have an
        associated program atom. The value of the atom is set to zero.

        Parameters
        ----------
        symbol
            The symbolic representation of the atom.
        atom
            The associated program atom (`0` for facts).
        """

    def output_term(self, symbol: Symbol, condition: Sequence[int]) -> None:
        """
        Observe shown terms passed to the solver.

        Parameters
        ----------
        symbol
            The symbolic representation of the term.
        condition
            List of program literals forming the condition when to show the
            term.
        """

    def external(self, atom: int, value: TruthValue) -> None:
        """
        Observe external statements passed to the solver.

        Parameters
        ----------
        atom
            The external atom in form of a program literal.
        value
            The truth value of the external statement.
        """

    def assume(self, literals: Sequence[int]) -> None:
        """
        Observe assumption directives passed to the solver.

        Parameters
        ----------
        literals
            The program literals to assume (positive literals are true and
            negative literals false for the next solve call).
        """

    def heuristic(
        self,
        atom: int,
        type_: HeuristicType,
        bias: int,
        priority: int,
        condition: Sequence[int],
    ) -> None:
        """
        Observe heuristic directives passed to the solver.

        Parameters
        ----------
        atom
            The program atom heuristically modified.
        type_
            The type of the modification.
        bias
            A signed integer.
        priority
            An unsigned integer.
        condition
            List of program literals.
        """

    def acyc_edge(self, node_u: int, node_v: int, condition: Sequence[int]) -> None:
        """
        Observe edge directives passed to the solver.

        Parameters
        ----------
        node_u
            The start vertex of the edge (in form of an integer).
        node_v
            Тhe end vertex of the edge (in form of an integer).
        condition
            The list of program literals forming th condition under which to
            add the edge.
        """

    def theory_term_number(self, term_id: int, number: int) -> None:
        """
        Observe numeric theory terms.

        Parameters
        ----------
        term_id
            The id of the term.
        number
            The value of the term.
        """

    def theory_term_string(self, term_id: int, name: str) -> None:
        """
        Observe string theory terms.

        Parameters
        ----------
        term_id
            The id of the term.
        name
            The string value of the term.
        """

    def theory_term_compound(
        self, term_id: int, name_id_or_type: int, arguments: Sequence[int]
    ) -> None:
        """
        Observe compound theory terms.

        Parameters
        ----------
        term_id
            The id of the term.
        name_id_or_type
            The name id or type of the term where the value `-1` stands for
            tuples, `-2` for sets, `-3` for lists, or otherwise for the id of
            the name (in form of a string term).
        arguments
            The arguments of the term in form of a list of term ids.
        """

    def theory_element(
        self, element_id: int, terms: Sequence[int], condition: Sequence[int]
    ) -> None:
        """
        Observe theory elements.

        Parameters
        ----------
        element_id
            The id of the element.
        terms
            The term tuple of the element in form of a list of term ids.
        condition
            The list of program literals forming the condition.
        """

    def theory_atom(
        self, atom_id_or_zero: int, term_id: int, elements: Sequence[int]
    ) -> None:
        """
        Observe theory atoms without guard.

        Parameters
        ----------
        atom_id_or_zero
            The id of the atom or zero for directives.
        term_id
            The term associated with the atom.
        elements
            The elements of the atom in form of a list of element ids.
        """

    def theory_atom_with_guard(
        self,
        atom_id_or_zero: int,
        term_id: int,
        elements: Sequence[int],
        operator_id: int,
        right_hand_side_id: int,
    ) -> None:
        """
        Observe theory atoms with guard.

        Parameters
        ----------
        atom_id_or_zero
            The id of the atom or zero for directives.
        term_id : int
            The term associated with the atom.
        elements
            The elements of the atom in form of a list of element ids.
        operator_id
            The id of the operator (a string term).
        right_hand_side_id
            The id of the term on the right hand side of the atom.
        """

    def end_step(self) -> None:
        """
        Marks the end of a block of directives passed to the solver.

        This function is called right before solving starts.
        """


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_init_program"
)
def _pyclingo_observer_init_program(incremental, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.init_program(incremental)
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_begin_step")
def _pyclingo_observer_begin_step(data):
    observer: Observer = _ffi.from_handle(data).data
    observer.begin_step()
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_end_step")
def _pyclingo_observer_end_step(data):
    observer: Observer = _ffi.from_handle(data).data
    observer.end_step()
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_rule")
def _pyclingo_observer_rule(choice, head, head_size, body, body_size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.rule(
        choice, [head[i] for i in range(head_size)], [body[i] for i in range(body_size)]
    )
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_weight_rule"
)
def _pyclingo_observer_weight_rule(
    choice, head, head_size, lower_bound, body, body_size, data
):
    observer: Observer = _ffi.from_handle(data).data
    observer.weight_rule(
        choice,
        [head[i] for i in range(head_size)],
        lower_bound,
        [(body[i].literal, body[i].weight) for i in range(body_size)],
    )
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_minimize")
def _pyclingo_observer_minimize(priority, literals, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.minimize(
        priority, [(literals[i].literal, literals[i].weight) for i in range(size)]
    )
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_project")
def _pyclingo_observer_project(atoms, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.project([atoms[i] for i in range(size)])
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_output_atom"
)
def _pyclingo_observer_output_atom(symbol, atom, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.output_atom(Symbol(symbol), atom)
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_output_term"
)
def _pyclingo_observer_output_term(symbol, condition, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.output_term(Symbol(symbol), [condition[i] for i in range(size)])
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_external")
def _pyclingo_observer_external(atom, type_, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.external(atom, TruthValue(type_))
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_assume")
def _pyclingo_observer_assume(literals, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.assume([literals[i] for i in range(size)])
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_heuristic")
def _pyclingo_observer_heuristic(atom, type_, bias, priority, condition, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.heuristic(
        atom, HeuristicType(type_), bias, priority, [condition[i] for i in range(size)]
    )
    return True


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_observer_acyc_edge")
def _pyclingo_observer_acyc_edge(node_u, node_v, condition, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.acyc_edge(node_u, node_v, [condition[i] for i in range(size)])
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_term_number"
)
def _pyclingo_observer_theory_term_number(term_id, number, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_term_number(term_id, number)
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_term_string"
)
def _pyclingo_observer_theory_term_string(term_id, name, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_term_string(term_id, _to_str(name))
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_term_compound"
)
def _pyclingo_observer_theory_term_compound(
    term_id, name_id_or_type, arguments, size, data
):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_term_compound(
        term_id, name_id_or_type, [arguments[i] for i in range(size)]
    )
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_element"
)
def _pyclingo_observer_theory_element(
    element_id, terms, terms_size, condition, condition_size, data
):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_element(
        element_id,
        [terms[i] for i in range(terms_size)],
        [condition[i] for i in range(condition_size)],
    )
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_atom"
)
def _pyclingo_observer_theory_atom(atom_id_or_zero, term_id, elements, size, data):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_atom(atom_id_or_zero, term_id, [elements[i] for i in range(size)])
    return True


@_ffi.def_extern(
    onerror=_cb_error_handler("data"), name="pyclingo_observer_theory_atom_with_guard"
)
def _pyclingo_observer_theory_atom_with_guard(
    atom_id_or_zero, term_id, elements, size, operator_id, right_hand_side_id, data
):
    observer: Observer = _ffi.from_handle(data).data
    observer.theory_atom_with_guard(
        atom_id_or_zero,
        term_id,
        [elements[i] for i in range(size)],
        operator_id,
        right_hand_side_id,
    )
    return True


class Backend(ContextManager["Backend"]):
    """
    Backend object providing a low level interface to extend a logic program.

    This class allows for adding statements in ASPIF format.

    See Also
    --------
    clingo.control.Control.backend

    Notes
    -----
    The `Backend` is a context manager and must be used with Python's `with`
    statement.
    """

    def __init__(self, rep, error):
        self._rep = rep
        self._error = error

    def __enter__(self):
        self._error.clear()
        _handle_error(_lib.clingo_backend_begin(self._rep), handler=self._error)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._error.clear()
        _handle_error(_lib.clingo_backend_end(self._rep), handler=self._error)
        return False

    def add_acyc_edge(self, node_u: int, node_v: int, condition: Sequence[int]) -> None:
        """
        Add an edge directive to the program.

        Parameters
        ----------
        node_u
            The start node represented as an unsigned integer.
        node_v
            The end node represented as an unsigned integer.
        condition
            List of program literals.
        """
        _handle_error(
            _lib.clingo_backend_acyc_edge(
                self._rep, node_u, node_v, condition, len(condition)
            )
        )

    def add_assume(self, literals: Sequence[int]) -> None:
        """
        Add assumptions to the program.

        Parameters
        ----------
        literals
            The list of literals to assume true.
        """
        _handle_error(_lib.clingo_backend_assume(self._rep, literals, len(literals)))

    def add_atom(self, symbol: Optional[Symbol] = None) -> int:
        """
        Return a fresh program atom or the atom associated with the given symbol.

        If the given symbol does not exist in the atom base, it is added first. Such
        atoms will be used in subequents calls to ground for instantiation.

        Parameters
        ----------
        symbol
            The symbol associated with the atom.
        Returns
        -------
        The program atom representing the atom.
        """
        # pylint: disable=protected-access
        if symbol is None:
            p_sym = _ffi.NULL
        else:
            p_sym = _ffi.new("clingo_symbol_t*", symbol._rep)

        self._error.clear()
        return _c_call(
            "clingo_atom_t",
            _lib.clingo_backend_add_atom,
            self._rep,
            p_sym,
            handler=self._error,
        )

    def add_external(self, atom: int, value: TruthValue = TruthValue.False_) -> None:
        """
        Mark a program atom as external optionally fixing its truth value.

        Parameters
        ----------
        atom
            The program atom to mark as external.
        value
            Optional truth value.

        Notes
        -----
        Can also be used to release an external atom using `TruthValue.Release`.
        """
        _handle_error(_lib.clingo_backend_external(self._rep, atom, value.value))

    def add_heuristic(
        self,
        atom: int,
        type_: HeuristicType,
        bias: int,
        priority: int,
        condition: Sequence[int],
    ) -> None:
        """
        Add a heuristic directive to the program.

        Parameters
        ----------
        atom
            Program atom to heuristically modify.
        type_
            The type of modification.
        bias
            A signed integer.
        priority
            An unsigned integer.
        condition
            List of program literals.
        """
        _handle_error(
            _lib.clingo_backend_heuristic(
                self._rep, atom, type_.value, bias, priority, condition, len(condition)
            )
        )

    def add_minimize(self, priority: int, literals: Sequence[Tuple[int, int]]) -> None:
        """
        Add a minimize constraint to the program.

        Parameters
        ----------
        priority
            Integer for the priority.
        literals
            List of pairs of program literals and weights.
        """
        _handle_error(
            _lib.clingo_backend_minimize(self._rep, priority, literals, len(literals))
        )

    def add_project(self, atoms: Sequence[int]) -> None:
        """
        Add a project statement to the program.

        Parameters
        ----------
        atoms
            List of program atoms to project on.
        """
        _handle_error(_lib.clingo_backend_project(self._rep, atoms, len(atoms)))

    def add_rule(
        self, head: Sequence[int], body: Sequence[int] = [], choice: bool = False
    ) -> None:
        """
        Add a disjuntive or choice rule to the program.

        Parameters
        ----------
        head
            The program atoms forming the rule head.
        body
            The program literals forming the rule body.
        choice
            Whether to add a disjunctive or choice rule.

        Notes
        -----
        Integrity constraints and normal rules can be added by using an empty or
        singleton head list, respectively.
        """
        # pylint: disable=dangerous-default-value
        _handle_error(
            _lib.clingo_backend_rule(
                self._rep, choice, head, len(head), body, len(body)
            )
        )

    def add_weight_rule(
        self,
        head: Sequence[int],
        lower: int,
        body: Sequence[Tuple[int, int]],
        choice: bool = False,
    ) -> None:
        """
        Add a disjuntive or choice rule with one weight constraint with a lower bound
        in the body to the program.

        Parameters
        ----------
        head
            The program atoms forming the rule head.
        lower
            The lower bound.
        body
            The pairs of program literals and weights forming the elements of the
            weight constraint.
        choice
            Whether to add a disjunctive or choice rule.
        """
        _handle_error(
            _lib.clingo_backend_weight_rule(
                self._rep, choice, head, len(head), lower, body, len(body)
            )
        )

    def add_theory_term_number(self, number: int) -> int:
        """
        Add a numeric theory term.

        Parameters
        ----------
        number
            The value of the term.

        Returns
        -------
        The id of the added term.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_term_number,
            self._rep,
            number,
            handler=self._error,
        )

    def add_theory_term_string(self, string: str) -> int:
        """
        Add a theory term representing a string.

        Parameters
        ----------
        string
            The value of the term.

        Returns
        -------
        The id of the added term.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_term_string,
            self._rep,
            string.encode(),
            handler=self._error,
        )

    def add_theory_term_function(self, name: str, arguments: Sequence[int]) -> int:
        """
        Add a theory term representing a function.

        Parameters
        ----------
        name
            The name of the function.
        arguments
            Sequence of term ids for the function arguments.

        Returns
        -------
        The id of the added term.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_term_function,
            self._rep,
            name.encode(),
            arguments,
            len(arguments),
            handler=self._error,
        )

    def add_theory_term_sequence(
        self, sequence_type: TheorySequenceType, arguments: Sequence[int]
    ) -> int:
        """
        Add a theory term representing a sequence of theory terms.

        Parameters
        ----------
        sequence_type
            The type of the sequence
        arguments
            Sequence of term ids for the function arguments.

        Returns
        -------
        The id of the added term.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_term_sequence,
            self._rep,
            sequence_type.value,
            arguments,
            len(arguments),
            handler=self._error,
        )

    def add_theory_term_symbol(self, symbol: Symbol) -> int:
        """
        Create a theory term from a symbol.

        Parameters
        ----------
        symbol
            The symbol to convert.

        Returns
        -------
        The id of the added term.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_term_symbol,
            self._rep,
            symbol._rep,
            handler=self._error,
        )

    def add_theory_element(self, terms: Sequence[int], condition: Sequence[int]) -> int:
        """
        Create a theory atom element.

        Parameters
        ----------
        terms
            A sequence of term ids.
        condition
            A sequence of program literals.

        Returns
        -------
        The id of the added element.
        """
        return _c_call(
            "clingo_id_t",
            _lib.clingo_backend_theory_element,
            self._rep,
            terms,
            len(terms),
            condition,
            len(condition),
            handler=self._error,
        )

    def add_theory_atom(
        self,
        term_id: int,
        elements: Sequence[int],
        atom_id_or_zero: Optional[int] = None,
    ) -> int:
        """
        Add a theory atom without a guard.

        If no atom_id is given, a fresh atom id is assigned.

        In case an atom id is given and an equivalent theory atom already
        exists, the given atom id is ignored.

        To declare a defined theory atom, a rule defining the program atom
        should be added. Otherwise, the theory atom is considered an external
        body occurrence.

        Parameters
        ----------
        term_id
            The id of the term associated with the theory atom.
        elements
            A sequence of ids of theory atom elements.
        atom_id_or_zero
            An optional program atom or zero for theory directives.
        """
        if atom_id_or_zero is None:
            atom_id_or_zero = 0xffffffff
        return _c_call(
            "clingo_atom_t",
            _lib.clingo_backend_theory_atom,
            self._rep,
            atom_id_or_zero,
            term_id,
            elements,
            len(elements),
            handler=self._error,
        )

    def add_theory_atom_with_guard(
        self,
        term_id: int,
        elements: Sequence[int],
        operator: str,
        right_hand_side_id: int,
        atom_id_or_zero: Optional[int] = None,
    ) -> int:
        """
        Add a theory atom with a guard.

        Parameters
        ----------
        term_id
            The id of the term associated with the theory atom.
        elements
            A sequence of ids of theory atom elements.
        operator
            String representing a theory operator.
        right_hand_side_id
            Term id for the term on the right hand side.
        atom_id_or_zero
            A optional program atom or zero for theory directives.

        See Also
        --------
        Backend.add_theory_atom
        """
        if atom_id_or_zero is None:
            atom_id_or_zero = 0xffffffff
        return _c_call(
            "clingo_atom_t",
            _lib.clingo_backend_theory_atom_with_guard,
            self._rep,
            atom_id_or_zero,
            term_id,
            elements,
            len(elements),
            operator.encode(),
            right_hand_side_id,
            handler=self._error,
        )
