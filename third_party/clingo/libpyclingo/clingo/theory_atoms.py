'''
Functions and classes to work with theory atoms.

Examples
--------
    >>> from clingo.control import Control
    >>>
    >>> ctl = Control()
    >>> ctl.add('base', [], """\\
    ... #theory example {
    ...     t { };
    ...     &a/0 : t, head
    ... }.
    ... {c}.
    ... &a { t: c }.
    ... """)
    >>> ctl.ground([('base', [])])
    >>> atm = next(ctl.theory_atoms)
    >>> print(atm)
    &a{t: c}
    >>> elm = atm.elements[0]
    >>> print(elm)
    t: c
'''

from typing import List, Optional, Tuple
from functools import total_ordering

from ._internal import _c_call, _c_call2, _lib, _str, _to_str
from .core import OrderedEnum

__all__ = ["TheoryAtom", "TheoryElement", "TheoryTerm", "TheoryTermType"]


class TheoryTermType(OrderedEnum):
    """
    Enumeration of theory term types.
    """

    Function = _lib.clingo_theory_term_type_function
    """
    For a function theory terms.
    """
    List = _lib.clingo_theory_term_type_list
    """
    For list theory terms.
    """
    Number = _lib.clingo_theory_term_type_number
    """
    For numeric theory terms.
    """
    Set = _lib.clingo_theory_term_type_set
    """
    For set theory terms.
    """
    Symbol = _lib.clingo_theory_term_type_symbol
    """
    For symbolic theory terms (symbol here means the term is a string).
    """
    Tuple = _lib.clingo_theory_term_type_tuple
    """
    For tuple theory terms.
    """


@total_ordering
class TheoryTerm:
    """
    `TheoryTerm` objects represent theory terms.

    Theory terms have a readable string representation, implement Python's rich
    comparison operators, and can be used as dictionary keys.
    """

    def __init__(self, rep, idx):
        self._rep = rep
        self._idx = idx

    def __hash__(self):
        return self._idx

    def __eq__(self, other):
        return self._idx == other._idx

    def __lt__(self, other):
        return self._idx < other._idx

    def __str__(self):
        return _str(
            _lib.clingo_theory_atoms_term_to_string_size,
            _lib.clingo_theory_atoms_term_to_string,
            self._rep,
            self._idx,
        )

    def __repr__(self):
        return f"TheoryTerm({self._rep!r})"

    @property
    def arguments(self) -> List["TheoryTerm"]:
        """
        The arguments of the term (for functions, tuples, list, and sets).
        """
        args, size = _c_call2(
            "clingo_id_t*",
            "size_t",
            _lib.clingo_theory_atoms_term_arguments,
            self._rep,
            self._idx,
        )
        return [TheoryTerm(self._rep, args[i]) for i in range(size)]

    @property
    def name(self) -> str:
        """
        The name of the term (for symbols and functions).
        """
        return _to_str(
            _c_call("char*", _lib.clingo_theory_atoms_term_name, self._rep, self._idx)
        )

    @property
    def number(self) -> int:
        """
        The numeric representation of the term (for numbers).
        """
        return _c_call(
            "int", _lib.clingo_theory_atoms_term_number, self._rep, self._idx
        )

    @property
    def type(self) -> TheoryTermType:
        """
        The type of the theory term.
        """
        type_ = _c_call(
            "clingo_theory_term_type_t",
            _lib.clingo_theory_atoms_term_type,
            self._rep,
            self._idx,
        )
        return TheoryTermType(type_)


@total_ordering
class TheoryElement:
    """
    Class to represent theory elements.

    Theory elements have a readable string representation, implement Python's rich
    comparison operators, and can be used as dictionary keys.
    """

    def __init__(self, rep, idx):
        self._rep = rep
        self._idx = idx

    def __hash__(self):
        return self._idx

    def __eq__(self, other):
        return self._idx == other._idx

    def __lt__(self, other):
        return self._idx < other._idx

    def __str__(self):
        return _str(
            _lib.clingo_theory_atoms_element_to_string_size,
            _lib.clingo_theory_atoms_element_to_string,
            self._rep,
            self._idx,
        )

    def __repr__(self):
        return f"TheoryElement({self._rep!r})"

    @property
    def condition(self) -> List[int]:
        """
        The condition of the element in form of a list of program literals.
        """
        cond, size = _c_call2(
            "clingo_literal_t*",
            "size_t",
            _lib.clingo_theory_atoms_element_condition,
            self._rep,
            self._idx,
        )
        return [cond[i] for i in range(size)]

    @property
    def condition_id(self) -> int:
        """
        Each condition has an id, which is a temporary program literal. This id
        can be passed to `clingo.propagator.PropagateInit.solver_literal` to
        obtain a corresponding solver literal.
        """
        return _c_call(
            "clingo_literal_t",
            _lib.clingo_theory_atoms_element_condition_id,
            self._rep,
            self._idx,
        )

    @property
    def terms(self) -> List[TheoryTerm]:
        """
        The tuple of the element.
        """
        terms, size = _c_call2(
            "clingo_id_t*",
            "size_t",
            _lib.clingo_theory_atoms_element_tuple,
            self._rep,
            self._idx,
        )
        return [TheoryTerm(self._rep, terms[i]) for i in range(size)]


@total_ordering
class TheoryAtom:
    """
    Class to represent theory atoms.

    Theory atoms have a readable string representation, implement Python's rich
    comparison operators, and can be used as dictionary keys.
    """

    def __init__(self, rep, idx):
        self._rep = rep
        self._idx = idx

    def __hash__(self):
        return self._idx

    def __eq__(self, other):
        return self._idx == other._idx

    def __lt__(self, other):
        return self._idx < other._idx

    def __str__(self):
        return _str(
            _lib.clingo_theory_atoms_atom_to_string_size,
            _lib.clingo_theory_atoms_atom_to_string,
            self._rep,
            self._idx,
        )

    def __repr__(self):
        return f"TheoryAtom({self._rep!r})"

    @property
    def elements(self) -> List[TheoryElement]:
        """
        The elements of the atom.
        """
        elems, size = _c_call2(
            "clingo_id_t*",
            "size_t",
            _lib.clingo_theory_atoms_atom_elements,
            self._rep,
            self._idx,
        )
        return [TheoryElement(self._rep, elems[i]) for i in range(size)]

    @property
    def guard(self) -> Optional[Tuple[str, TheoryTerm]]:
        """
        The guard of the atom or None if the atom has no guard.
        """
        if not _c_call(
            "bool", _lib.clingo_theory_atoms_atom_has_guard, self._rep, self._idx
        ):
            return None

        conn, term = _c_call2(
            "char*",
            "clingo_id_t",
            _lib.clingo_theory_atoms_atom_guard,
            self._rep,
            self._idx,
        )

        return (_to_str(conn), TheoryTerm(self._rep, term))

    @property
    def literal(self) -> int:
        """
        The program literal associated with the atom.
        """
        return _c_call(
            "clingo_literal_t",
            _lib.clingo_theory_atoms_atom_literal,
            self._rep,
            self._idx,
        )

    @property
    def term(self) -> TheoryTerm:
        """
        The term of the atom.
        """
        term = _c_call(
            "clingo_id_t", _lib.clingo_theory_atoms_atom_term, self._rep, self._idx
        )
        return TheoryTerm(self._rep, term)
