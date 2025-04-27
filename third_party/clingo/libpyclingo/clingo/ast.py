"""
Module to work with clingo's non-ground program representation.

Grammar
-------
The grammar below defines valid ASTs. For each upper case identifier there is a
matching function in the module. Arguments follow in parenthesis: each having a
type given on the right-hand side of the colon. The symbols `?`, `*`, and `+`
are used to denote optional arguments (`None` encodes abscence), list
arguments, and non-empty list arguments.

```
# Terms

term = SymbolicTerm
        ( location : Location
        , symbol   : clingo.Symbol
        )
     | Variable
        ( location : Location
        , name     : str
        )
     | UnaryOperation
        ( location      : Location
        , operator_type : UnaryOperator
        , argument      : term
        )
     | BinaryOperation
        ( location      : Location
        , operator_type : BinaryOperator
        , left          : term
        , right         : term
        )
     | Interval
        ( location : Location
        , left     : term
        , right    : term
        )
     | Function
        ( location  : Location
        , name      : str
        , arguments : term*
        , external  : bool
        )
     | Pool
        ( location  : Location
        , arguments : term*
        )

theory_term = SymbolicTerm
               ( location : Location
               , symbol   : clingo.Symbol
               )
            | Variable
               ( location : Location
               , name     : str
               )
            | TheorySequence
               ( location : Location
               , sequence_type : TheorySequenceType
               , terms         : theory_term*
               )
            | TheoryFunction
               ( location  : Location
               , name      : str
               , arguments : theory_term*
               )
            | TheoryUnparsedTerm
               ( location : Location
               , elements : TheoryUnparsedTermElement
                             ( operators : str*
                             , term      : theory_term
                             )+
               )

# Literals

symbolic_atom = SymbolicAtom
                 ( symbol : term
                 )

guard = Guard
         ( comparison : ComparisonOperator
         , term       : term
         )

literal = Literal
           ( location : Location
           , sign     : Sign
           , atom     : Comparison
                         ( term   : term
                         , guards : guard+
                         )
                      | BooleanConstant
                         ( value : bool
                         )
                      | symbolic_atom
           )

# Head and Body Literals

conditional_literal = ConditionalLiteral
                       ( location  : Location
                       , literal   : Literal
                       , condition : Literal*
                       )

aggregate = Aggregate
             ( location    : Location
             , left_guard  : guard?
             , elements    : conditional_literal*
             , right_guard : guard?
             )

theory_atom = TheoryAtom
               ( location : Location
               , term     : term
               , elements : TheoryAtomElement
                             ( terms     : theory_term*
                             , condition : literal*
                             )*
               , guard    : TheoryGuard
                             ( operator_name : str
                             , term          : theory_term
                             )?
               )

body_atom = aggregate
          | BodyAggregate
             ( location    : Location
             , left_guard  : guard?
             , function    : AggregateFunction
             , elements    : BodyAggregateElement
                              ( terms     : term*
                              , condition : literal*
                              )*
             , right_guard : guard?
             )
          | theory_atom

body_literal = literal
             | conditional_literal
             | Literal
                ( location : Location
                , sign     : Sign
                , atom     : body_atom
                )

head = literal
     | aggregate
     | HeadAggregate
        ( location    : Location
        , left_guard  : guard?
        , function    : AggregateFunction
        , elements    : HeadAggregateElement
                         ( terms     : term*
                         , condition : conditional_literal
                         )*
        , right_guard : guard?
        )
     | Disjunction
        ( location : Location
        , elements : conditional_literal*
        )
     | theory_atom

# Statements

statement = Rule
             ( location : Location
             , head     : head
             , body     : body_literal*
             )
          | Definition
             ( location   : Location
             , name       : str
             , value      : term
             , is_default : bool
             )
          | ShowSignature
             ( location   : Location
             , name       : str
             , arity      : int
             , sign       : bool
             )
          | Defined
             ( location   : Location
             , name       : str
             , arity      : int
             , sign       : bool
             )
          | ShowTerm
             ( location : Location
             , term     : term
             , body     : body_literal*
             )
          | Minimize
             ( location : Location
             , weight   : term
             , priority : term
             , terms    : term*
             , body     : body_literal*
             )
          | Script
             ( location : Location
             , name     : str
             , code     : str
             )
          | Program
             ( location   : Location
             , name       : str
             , parameters : Id
                             ( location : Location
                             , id       : str
                             )*
             )
          | External
             ( location : Location
             , atom     : symbolic_atom
             , body     : body_literal*
             , type     : term
             )
          | Edge
             ( location : Location
             , u        : term
             , v        : term
             , body     : body_literal*
             )
          | Heuristic
             ( location : Location
             , atom     : symbolic_atom
             , body     : body_literal*
             , bias     : term
             , priority : term
             , modifier : term
             )
          | ProjectAtom
             ( location : Location
             , atom     : symbolic_atom
             , body     : body_literal*
             )
          | ProjectSignature
             ( location : Location
             , name     : str
             , arity    : int
             , sign     : bool
             )
          | TheoryDefinition
             ( location : Location
             , name     : str
             , terms    : TheoryTermDefinition
                           ( location  : Location
                           , name      : str
                           , operators : TheoryOperatorDefinition
                                          ( location      : Location
                                          , name          : str
                                          , priority      : int
                                          , operator_type : TheoryOperatorType
                                          )*
                           )*
             , atoms    : TheoryAtomDefinition
                           ( location  : Location
                           , atom_type : TheoryAtomType
                           , name      : str
                           , arity     : int
                           , term      : str
                           , guard     : TheoryGuardDefinition
                                          ( operators : str*
                                          , term      : str
                                          )?
                           )*
             )
          | Comment
             ( location     : Location
             , value        : str
             , comment_type : CommentType
             )
```

Examples
--------
The following example parses a program from a string and passes the resulting
`AST` to the builder:

    >>> from clingo import Control, ast
    >>> from clingo.ast import Location, ProgramBuilder, Position, parse_string
    >>>
    >>> ctl = Control()
    >>>
    >>> with ProgramBuilder(ctl) as bld:
    ...     # parse from string
    ...     parse_string('a.', bld.add)
    ...     # build rule manually
    ...     pos = Position('<string>', 1, 1)
    ...     loc = Location(pos, pos)
    ...     fun = ast.Function(loc, 'b', [], False)
    ...     atm = ast.SymbolicAtom(fun)
    ...     lit = ast.Literal(loc, ast.Sign.NoSign, atm)
    ...     bld.add(ast.Rule(loc, lit, []))
    ...
    >>> ctl.ground([('base', [])])
    >>> print(ctl.solve(on_model=print))
    a b
    SAT

The next example shows how to transform ASTs using the `Transformer` class:

    >>> from clingo.ast import Transformer, Variable, parse_string
    >>>
    >>> class VariableRenamer(Transformer):
    ...     def visit_Variable(self, node):
    ...         return node.update(name='_' + node.name)
    ...
    >>> vrt = VariableRenamer()
    >>> parse_string('p(X) :- q(X).', lambda stm: print(str(vrt(stm))))
    #program base.
    p(_X) :- q(_X).
"""

from enum import IntEnum
from typing import (
    Any,
    Callable,
    ContextManager,
    Dict,
    List,
    MutableSequence,
    NamedTuple,
    Optional,
    Sequence,
    Tuple,
    Union,
)
from collections import abc
from functools import total_ordering

from ._internal import (
    _CBData,
    _Error,
    _cb_error_handler,
    _c_call,
    _ffi,
    _handle_error,
    _lib,
    _str,
    _to_str,
)
from .util import Slice, SlicedMutableSequence
from .core import Logger, OrderedEnum
from .control import Control
from .symbol import Symbol

__all__ = [
    "AST",
    "ASTSequence",
    "ASTType",
    "ASTValue",
    "Aggregate",
    "AggregateFunction",
    "Guard",
    "BinaryOperation",
    "BinaryOperator",
    "BodyAggregate",
    "BodyAggregateElement",
    "BooleanConstant",
    "Comment",
    "CommentType",
    "Comparison",
    "ComparisonOperator",
    "ConditionalLiteral",
    "Defined",
    "Definition",
    "Disjunction",
    "Edge",
    "External",
    "Function",
    "HeadAggregate",
    "HeadAggregateElement",
    "Heuristic",
    "Id",
    "Interval",
    "Literal",
    "Location",
    "Minimize",
    "Pool",
    "Position",
    "Program",
    "ProgramBuilder",
    "ProjectAtom",
    "ProjectSignature",
    "Rule",
    "Script",
    "ShowSignature",
    "ShowTerm",
    "Sign",
    "StrSequence",
    "SymbolicAtom",
    "SymbolicTerm",
    "TheoryAtom",
    "TheoryAtomDefinition",
    "TheoryAtomElement",
    "TheoryAtomType",
    "TheoryDefinition",
    "TheoryFunction",
    "TheoryGuard",
    "TheoryGuardDefinition",
    "TheoryOperatorDefinition",
    "TheoryOperatorType",
    "TheorySequence",
    "TheorySequenceType",
    "TheoryTermDefinition",
    "TheoryUnparsedTerm",
    "TheoryUnparsedTermElement",
    "Transformer",
    "UnaryOperation",
    "UnaryOperator",
    "Variable",
    "parse_files",
    "parse_string",
]

# pylint: disable=protected-access,invalid-name,too-many-lines,too-many-ancestors


class ASTType(OrderedEnum):
    """
    Enumeration of ast node types.
    """

    Id = _lib.clingo_ast_type_id
    Variable = _lib.clingo_ast_type_variable
    SymbolicTerm = _lib.clingo_ast_type_symbolic_term
    UnaryOperation = _lib.clingo_ast_type_unary_operation
    BinaryOperation = _lib.clingo_ast_type_binary_operation
    Interval = _lib.clingo_ast_type_interval
    Function = _lib.clingo_ast_type_function
    Pool = _lib.clingo_ast_type_pool
    BooleanConstant = _lib.clingo_ast_type_boolean_constant
    SymbolicAtom = _lib.clingo_ast_type_symbolic_atom
    Comparison = _lib.clingo_ast_type_comparison
    Guard = _lib.clingo_ast_type_guard
    ConditionalLiteral = _lib.clingo_ast_type_conditional_literal
    Aggregate = _lib.clingo_ast_type_aggregate
    BodyAggregateElement = _lib.clingo_ast_type_body_aggregate_element
    BodyAggregate = _lib.clingo_ast_type_body_aggregate
    HeadAggregateElement = _lib.clingo_ast_type_head_aggregate_element
    HeadAggregate = _lib.clingo_ast_type_head_aggregate
    Disjunction = _lib.clingo_ast_type_disjunction
    TheorySequence = _lib.clingo_ast_type_theory_sequence
    TheoryFunction = _lib.clingo_ast_type_theory_function
    TheoryUnparsedTermElement = _lib.clingo_ast_type_theory_unparsed_term_element
    TheoryUnparsedTerm = _lib.clingo_ast_type_theory_unparsed_term
    TheoryGuard = _lib.clingo_ast_type_theory_guard
    TheoryAtomElement = _lib.clingo_ast_type_theory_atom_element
    TheoryAtom = _lib.clingo_ast_type_theory_atom
    Literal = _lib.clingo_ast_type_literal
    TheoryOperatorDefinition = _lib.clingo_ast_type_theory_operator_definition
    TheoryTermDefinition = _lib.clingo_ast_type_theory_term_definition
    TheoryGuardDefinition = _lib.clingo_ast_type_theory_guard_definition
    TheoryAtomDefinition = _lib.clingo_ast_type_theory_atom_definition
    Rule = _lib.clingo_ast_type_rule
    Definition = _lib.clingo_ast_type_definition
    ShowSignature = _lib.clingo_ast_type_show_signature
    ShowTerm = _lib.clingo_ast_type_show_term
    Minimize = _lib.clingo_ast_type_minimize
    Script = _lib.clingo_ast_type_script
    Program = _lib.clingo_ast_type_program
    External = _lib.clingo_ast_type_external
    Edge = _lib.clingo_ast_type_edge
    Heuristic = _lib.clingo_ast_type_heuristic
    ProjectAtom = _lib.clingo_ast_type_project_atom
    ProjectSignature = _lib.clingo_ast_type_project_signature
    Defined = _lib.clingo_ast_type_defined
    TheoryDefinition = _lib.clingo_ast_type_theory_definition
    Comment = _lib.clingo_ast_type_comment


class AggregateFunction(IntEnum):
    """
    Enumeration of aggegate functions.
    """

    Count = _lib.clingo_ast_aggregate_function_count
    """
    The `#count` function.
    """
    Max = _lib.clingo_ast_aggregate_function_max
    """
    The `#max` function.
    """
    Min = _lib.clingo_ast_aggregate_function_min
    """
    The `#min` function.
    """
    Sum = _lib.clingo_ast_aggregate_function_sum
    """
    The `#sum` function.
    """
    SumPlus = _lib.clingo_ast_aggregate_function_sump
    """
    The `#sum+` function.
    """


class BinaryOperator(IntEnum):
    """
    Enumeration of binary operators.
    """

    And = _lib.clingo_ast_binary_operator_and
    """
    For bitwise and.
    """
    Division = _lib.clingo_ast_binary_operator_division
    """
    For arithmetic division.
    """
    Minus = _lib.clingo_ast_binary_operator_minus
    """
    For arithmetic subtraction.
    """
    Modulo = _lib.clingo_ast_binary_operator_modulo
    """
    For arithmetic modulo.
    """
    Multiplication = _lib.clingo_ast_binary_operator_multiplication
    """
    For arithmetic multipilcation.
    """
    Or = _lib.clingo_ast_binary_operator_or
    """
    For bitwise or.
    """
    Plus = _lib.clingo_ast_binary_operator_plus
    """
    For arithmetic addition.
    """
    Power = _lib.clingo_ast_binary_operator_power
    """
    For arithmetic exponentiation.
    """
    XOr = _lib.clingo_ast_binary_operator_xor
    """
    For bitwise exclusive or.
    """


class CommentType(OrderedEnum):
    """
    Enumeration of comment types.
    """

    Line = _lib.clingo_comment_type_line
    """
    Line comments starting with `%` ending at a newline.
    """
    Block = _lib.clingo_comment_type_block
    """
    Block comments enclosed in `%*` and  `*%`.
    """


class ComparisonOperator(IntEnum):
    """
    Enumeration of comparison operators.
    """

    Equal = _lib.clingo_ast_comparison_operator_equal
    """
    The `=` operator
    """
    GreaterEqual = _lib.clingo_ast_comparison_operator_greater_equal
    """
    The `>=` operator.
    """
    GreaterThan = _lib.clingo_ast_comparison_operator_greater_than
    """
    The `>` operator.
    """
    LessEqual = _lib.clingo_ast_comparison_operator_less_equal
    """
    The `<=` operator.
    """
    LessThan = _lib.clingo_ast_comparison_operator_less_than
    """
    The `<` operator.
    """
    NotEqual = _lib.clingo_ast_comparison_operator_not_equal
    """
    The `!=` operator.
    """


class Sign(IntEnum):
    """
    Enumeration of signs for literals.
    """

    DoubleNegation = _lib.clingo_ast_sign_double_negation
    """
    For double negated literals (with prefix `not not`)
    """
    Negation = _lib.clingo_ast_sign_negation
    """
    For negative literals (with prefix `not`).
    """
    NoSign = _lib.clingo_ast_sign_no_sign
    """
    For positive literals.
    """


class TheoryAtomType(IntEnum):
    """
    Enumeration of theory atom types.
    """

    Any = _lib.clingo_ast_theory_atom_definition_type_any
    """
    For atoms that can occur anywhere in a rule.
    """
    Body = _lib.clingo_ast_theory_atom_definition_type_body
    """
    For atoms that can only occur in rule bodies.
    """
    Directive = _lib.clingo_ast_theory_atom_definition_type_directive
    """
    For atoms that can only occur in facts.
    """
    Head = _lib.clingo_ast_theory_atom_definition_type_head
    """
    For atoms that can only occur in rule heads.
    """


class TheoryOperatorType(IntEnum):
    """
    Enumeration of operator types.
    """

    BinaryLeft = _lib.clingo_ast_theory_operator_type_binary_left
    """
    For binary left associative operators.
    """
    BinaryRight = _lib.clingo_ast_theory_operator_type_binary_right
    """
    For binary right associative operator.
    """
    Unary = _lib.clingo_ast_theory_operator_type_unary
    """
    For unary operators.
    """


class TheorySequenceType(IntEnum):
    """
    Enumeration of theory term sequence types.
    """

    List = _lib.clingo_ast_theory_sequence_type_list
    """
    For sequences enclosed in brackets.
    """
    Set = _lib.clingo_ast_theory_sequence_type_set
    """
    For sequences enclosed in braces.
    """
    Tuple = _lib.clingo_ast_theory_sequence_type_tuple
    """
    For sequences enclosed in parenthesis.
    """


class UnaryOperator(IntEnum):
    """
    Enumeration of signs for literals.
    """

    Absolute = _lib.clingo_ast_unary_operator_absolute
    """
    For taking the absolute value.
    """
    Minus = _lib.clingo_ast_unary_operator_minus
    """
    For unary minus and classical negation.
    """
    Negation = _lib.clingo_ast_unary_operator_negation
    """
    For bitwise negation.
    """


class ASTSequence(abc.MutableSequence):
    """
    A sequence holding `AST` nodes.

    Sequences implement Python's rich comparison operators and are ordered
    structurally ignoring the location. They can also be used as dictionary
    keys.
    """

    def __init__(self, rep, attribute):
        self._rep = rep
        self._attribute = attribute
        _lib.clingo_ast_acquire(self._rep)

    def __eq__(self, other):
        return tuple(self) == tuple(other)

    def __lt__(self, other):
        return tuple(self) < tuple(other)

    def __hash__(self):
        return hash(tuple(self))

    def __del__(self):
        _lib.clingo_ast_release(self._rep)

    def __len__(self) -> int:
        return _c_call(
            "size_t",
            _lib.clingo_ast_attribute_size_ast_array,
            self._rep,
            self._attribute,
        )

    def __getitem__(self, index):
        if isinstance(index, slice):
            return SlicedMutableSequence(self, Slice(index))
        size = len(self)
        if index < 0:
            index += size
        if index < 0 or index >= size:
            raise IndexError("invalid index")
        return AST(
            _c_call(
                "clingo_ast_t*",
                _lib.clingo_ast_attribute_get_ast_at,
                self._rep,
                self._attribute,
                index,
            )
        )

    def __iter__(self):
        for index in range(len(self)):
            yield AST(
                _c_call(
                    "clingo_ast_t*",
                    _lib.clingo_ast_attribute_get_ast_at,
                    self._rep,
                    self._attribute,
                    index,
                )
            )

    def __setitem__(self, index, ast):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        _handle_error(
            _lib.clingo_ast_attribute_set_ast_at(
                self._rep, self._attribute, index, ast._rep
            )
        )

    def __delitem__(self, index):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        size = len(self)
        if index < 0:
            index += size
        if index < 0 or index >= size:
            raise IndexError("invalid index")
        _handle_error(
            _lib.clingo_ast_attribute_delete_ast_at(self._rep, self._attribute, index)
        )

    def insert(self, index, value):
        _handle_error(
            _lib.clingo_ast_attribute_insert_ast_at(
                self._rep, self._attribute, index, value._rep
            )
        )

    def clear(self):
        """
        Remove all elements from the sequence.
        """
        for i in range(len(self), 0, -1):
            del self[i - 1]

    def __str__(self):
        return str(list(self))

    def __repr__(self):
        return repr(list(self))


class StrSequence(abc.MutableSequence):
    """
    A sequence holding strings.
    """

    def __init__(self, rep, attribute):
        self._attribute = attribute
        self._rep = rep
        _lib.clingo_ast_acquire(self._rep)

    def __del__(self):
        _lib.clingo_ast_release(self._rep)

    def __len__(self) -> int:
        return _c_call(
            "size_t",
            _lib.clingo_ast_attribute_size_string_array,
            self._rep,
            self._attribute,
        )

    def __getitem__(self, index):
        if isinstance(index, slice):
            return SlicedMutableSequence(self, Slice(index))
        size = len(self)
        if index < 0:
            index += size
        if index < 0 or index >= size:
            raise IndexError("invalid index")
        return _to_str(
            _c_call(
                "char*",
                _lib.clingo_ast_attribute_get_string_at,
                self._rep,
                self._attribute,
                index,
            )
        )

    def __iter__(self):
        for index in range(len(self)):
            yield _to_str(
                _c_call(
                    "char*",
                    _lib.clingo_ast_attribute_get_string_at,
                    self._rep,
                    self._attribute,
                    index,
                )
            )

    def __setitem__(self, index, value):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        _handle_error(
            _lib.clingo_str_attribute_set_string_at(
                self._rep, self._attribute, index, value.encode()
            )
        )

    def __delitem__(self, index):
        if isinstance(index, slice):
            raise TypeError("slicing not implemented")
        size = len(self)
        if index < 0:
            index += size
        if index < 0 or index >= size:
            raise IndexError("invalid index")
        _handle_error(
            _lib.clingo_ast_attribute_delete_string_at(
                self._rep, self._attribute, index
            )
        )

    def insert(self, index, value):
        _handle_error(
            _lib.clingo_ast_attribute_insert_string_at(
                self._rep, self._attribute, index, value.encode()
            )
        )

    def clear(self):
        """
        Remove all elements from the sequence.
        """
        for i in range(len(self), 0, -1):
            del self[i - 1]

    def __str__(self):
        return str(list(self))

    def __repr__(self):
        return repr(list(self))


class Position(NamedTuple):
    """
    Class to point to a position in a text file.
    """

    filename: str
    """
    The file name.
    """
    line: int
    """
    The line number in the file.
    """
    column: int
    """
    The column number in the line.
    """


class Location(NamedTuple):
    """
    Class to point to a range in a text file.
    """

    begin: Position
    """
    The beginning of the range.
    """
    end: Position
    """
    The end of the range.
    """


def _c_location(loc: Location):
    mema = _ffi.new("char[]", loc.begin.filename.encode())
    memb = _ffi.new("char[]", loc.end.filename.encode())
    return (
        _ffi.new(
            "clingo_location_t*",
            (
                mema,
                memb,
                loc.begin.line,
                loc.end.line,
                loc.begin.column,
                loc.end.column,
            ),
        ),
        mema,
        memb,
    )


def _py_location(rep):
    return Location(
        Position(_to_str(rep.begin_file), rep.begin_line, rep.begin_column),
        Position(_to_str(rep.end_file), rep.end_line, rep.end_column),
    )


_attribute_names = {
    _to_str(_lib.g_clingo_ast_attribute_names.names[i]): i
    for i in range(_lib.g_clingo_ast_attribute_names.size)
}

ASTValue = Union[str, int, Symbol, Location, None, "AST", StrSequence, ASTSequence]
ASTUpdate = Union[
    str, int, Symbol, Location, None, "AST", Sequence[str], Sequence["AST"]
]


@total_ordering
class AST:
    """
    Represents a node in the abstract syntax tree.

    The attributes of an `AST` are tied to its type. They correspond to the
    grammar in the description of the `clingo.ast` module. `AST` nodes can be
    constructed using one of the functions provided in this module.

    Furthermore, AST nodes implement Python's rich comparison operators and are
    ordered structurally ignoring the location. They can also be used as
    dictionary keys. Their string representation corresponds to their gringo
    representation. In fact, the string representation of any AST obtained from
    `parse_files` and `parse_string` can be parsed again. Note that it is
    possible to construct ASTs that are not parsable, though.
    """

    def __init__(self, rep):
        super().__setattr__("_rep", rep)

    def __eq__(self, other):
        if not isinstance(other, AST):
            return NotImplemented
        return _lib.clingo_ast_equal(self._rep, other._rep)

    def __lt__(self, other):
        if not isinstance(other, AST):
            return NotImplemented
        return _lib.clingo_ast_less_than(self._rep, other._rep)

    def __hash__(self):
        return _lib.clingo_ast_hash(self._rep)

    def __del__(self):
        _lib.clingo_ast_release(self._rep)

    def __getattr__(self, name):
        attr_id = _attribute_names.get(name)
        if attr_id is None or not _c_call(
            "bool", _lib.clingo_ast_has_attribute, self._rep, attr_id
        ):
            raise AttributeError(f"no attribute: {name}")
        attr_type = _c_call(
            "clingo_ast_attribute_type_t",
            _lib.clingo_ast_attribute_type,
            self._rep,
            attr_id,
        )
        if attr_type == _lib.clingo_ast_attribute_type_string:
            return _to_str(
                _c_call(
                    "char*", _lib.clingo_ast_attribute_get_string, self._rep, attr_id
                )
            )
        if attr_type == _lib.clingo_ast_attribute_type_number:
            return _c_call(
                "int", _lib.clingo_ast_attribute_get_number, self._rep, attr_id
            )
        if attr_type == _lib.clingo_ast_attribute_type_symbol:
            return Symbol(
                _c_call(
                    "clingo_symbol_t",
                    _lib.clingo_ast_attribute_get_symbol,
                    self._rep,
                    attr_id,
                )
            )
        if attr_type == _lib.clingo_ast_attribute_type_location:
            return _py_location(
                _c_call(
                    "clingo_location_t",
                    _lib.clingo_ast_attribute_get_location,
                    self._rep,
                    attr_id,
                )
            )
        if attr_type == _lib.clingo_ast_attribute_type_optional_ast:
            rep = _c_call(
                "clingo_ast_t*",
                _lib.clingo_ast_attribute_get_optional_ast,
                self._rep,
                attr_id,
            )
            return AST(rep) if rep != _ffi.NULL else None
        if attr_type == _lib.clingo_ast_attribute_type_ast:
            return AST(
                _c_call(
                    "clingo_ast_t*",
                    _lib.clingo_ast_attribute_get_ast,
                    self._rep,
                    attr_id,
                )
            )
        if attr_type == _lib.clingo_ast_attribute_type_string_array:
            return StrSequence(self._rep, attr_id)
        assert attr_type == _lib.clingo_ast_attribute_type_ast_array
        return ASTSequence(self._rep, attr_id)

    def __setattr__(self, name, value):
        attr_id = getattr(_lib, f"clingo_ast_attribute_{name}")
        if not _c_call("bool", _lib.clingo_ast_has_attribute, self._rep, attr_id):
            raise AttributeError(f"no attribute: {name}")
        attr_type = _c_call(
            "clingo_ast_attribute_type_t",
            _lib.clingo_ast_attribute_type,
            self._rep,
            attr_id,
        )
        if attr_type == _lib.clingo_ast_attribute_type_string:
            _handle_error(
                _lib.clingo_ast_attribute_set_string(self._rep, attr_id, value.encode())
            )
        elif attr_type == _lib.clingo_ast_attribute_type_number:
            _handle_error(
                _lib.clingo_ast_attribute_set_number(self._rep, attr_id, value)
            )
        elif attr_type == _lib.clingo_ast_attribute_type_symbol:
            _handle_error(
                _lib.clingo_ast_attribute_set_symbol(self._rep, attr_id, value._rep)
            )
        elif attr_type == _lib.clingo_ast_attribute_type_location:
            c_loc = _c_location(value)
            _handle_error(
                _lib.clingo_ast_attribute_set_location(self._rep, attr_id, c_loc[0])
            )
        elif attr_type == _lib.clingo_ast_attribute_type_optional_ast:
            _handle_error(
                _lib.clingo_ast_attribute_set_optional_ast(
                    self._rep, attr_id, _ffi.NULL if value is None else value._rep
                )
            )
        elif attr_type == _lib.clingo_ast_attribute_type_ast:
            _handle_error(
                _lib.clingo_ast_attribute_set_ast(self._rep, attr_id, value._rep)
            )
        elif attr_type == _lib.clingo_ast_attribute_type_string_array:
            if isinstance(value, StrSequence):
                if attr_id == value._attribute and self._rep == value._rep:
                    value = list(value)
            elif not isinstance(value, list):
                value = list(value)
            str_seq = StrSequence(self._rep, attr_id)
            str_seq.clear()
            str_seq.extend(value)
        else:
            assert attr_type == _lib.clingo_ast_attribute_type_ast_array
            if isinstance(value, ASTSequence):
                if attr_id == value._attribute and self._rep == value._rep:
                    value = list(value)
            elif not isinstance(value, list):
                value = list(value)
            ast_seq = ASTSequence(self._rep, attr_id)
            ast_seq.clear()
            ast_seq.extend(value)

    def __str__(self):
        return _str(
            _lib.clingo_ast_to_string_size, _lib.clingo_ast_to_string, self._rep
        )

    def __repr__(self):
        name = str(self.ast_type).replace("ASTType", "ast")
        args = ", ".join(repr(x) for x in self.values())
        return f"{name}({args})"

    def __copy__(self) -> "AST":
        """
        Return a shallow copy of the ast.
        """
        return AST(_c_call("clingo_ast_t*", _lib.clingo_ast_copy, self._rep))

    def __deepcopy__(self, memo) -> "AST":
        """
        Return a deep copy of the ast.
        """
        return AST(_c_call("clingo_ast_t*", _lib.clingo_ast_deep_copy, self._rep))

    def update(self, **kwargs: ASTUpdate) -> "AST":
        """
        Return a copy of the AST also updating the given attributes.

        Note that this function returns a reference to self if no arguments are
        given.
        """
        if not kwargs:
            return self

        args = []
        for key in self.keys():
            if key in kwargs:
                args.append(kwargs[key])
            else:
                args.append(getattr(self, key))

        cons = globals()[str(self.ast_type).replace("ASTType.", "")]
        return cons(*args)

    def items(self) -> List[Tuple[str, ASTValue]]:
        """
        The list of items of the AST node.
        """
        return [(name, getattr(self, name)) for name in self.keys()]

    def keys(self) -> List[str]:
        """
        The list of keys of the AST node.
        """
        cons = _lib.g_clingo_ast_constructors.constructors[self.ast_type.value]
        names = _lib.g_clingo_ast_attribute_names.names
        return [_to_str(names[cons.arguments[j].attribute]) for j in range(cons.size)]

    def values(self) -> List[ASTValue]:
        """
        The list of values of the AST node.
        """
        return [(getattr(self, name)) for name in self.keys()]

    @property
    def ast_type(self) -> ASTType:
        """
        The type of the node.
        """
        return ASTType(
            _c_call("clingo_ast_type_t", _lib.clingo_ast_get_type, self._rep)
        )

    @property
    def child_keys(self) -> List[str]:
        """
        List of attribute names containing ASTs.
        """
        cons = _lib.g_clingo_ast_constructors.constructors[self.ast_type.value]
        names = _lib.g_clingo_ast_attribute_names.names
        return [
            _to_str(names[cons.arguments[j].attribute])
            for j in range(cons.size)
            if cons.arguments[j].type
            in (
                _lib.clingo_ast_attribute_type_ast,
                _lib.clingo_ast_attribute_type_optional_ast,
                _lib.clingo_ast_attribute_type_ast_array,
            )
        ]

    def unpool(self, other: bool = True, condition: bool = True) -> List["AST"]:
        """
        Unpool the AST returning a list of ASTs without pool terms.

        Parameters
        ----------
        other
            Remove all pools except those in conditions of conditional
            literals.
        condition
            Only remove pools from conditions of conditional literals.
        """
        unpool_type = 0
        if other:
            unpool_type |= _lib.clingo_ast_unpool_type_other
        if condition:
            unpool_type |= _lib.clingo_ast_unpool_type_condition

        ret: List[AST] = []
        error = _Error()
        cb_data = _CBData(ret.append, error)
        c_cb_data = _ffi.new_handle(cb_data)
        _handle_error(
            _lib.clingo_ast_unpool(
                self._rep, unpool_type, _lib.pyclingo_ast_callback, c_cb_data
            )
        )
        return ret


@_ffi.def_extern(onerror=_cb_error_handler("data"), name="pyclingo_ast_callback")
def _pyclingo_ast_callback(ast, data):
    """
    Low-level ast callback.
    """
    callback = _ffi.from_handle(data).data
    _lib.clingo_ast_acquire(ast)
    callback(AST(ast))

    return True


def parse_files(
    files: Sequence[str],
    callback: Callable[[AST], None],
    control: Optional[Control] = None,
    logger: Optional[Logger] = None,
    message_limit: int = 20,
) -> None:
    """
    Parse the programs in the given files and return an abstract syntax tree for
    each statement via a callback.

    The function follows clingo's handling of files on the command line. Filename
    `"-"` is treated as stdin and if an empty list is given, then the parser will
    read from stdin.

    The optional control object can be added to enable parsing of files in
    ASPIF format. The ground statements will be added to the control object.

    Parameters
    ----------
    files
        List of file names.
    callback
        Callable taking an ast as argument.
    control
        Control object to add ground rules to.
    logger
        Function to intercept messages normally printed to standard error.
    message_limit
        The maximum number of messages passed to the logger.

    See Also
    --------
    ProgramBuilder
    """
    c_control = control._rep if control is not None else _ffi.NULL

    if logger is not None:
        c_logger_data = _ffi.new_handle(logger)
        c_logger = _lib.pyclingo_logger_callback
    else:
        c_logger_data = _ffi.NULL
        c_logger = _ffi.NULL

    error = _Error()
    cb_data = _CBData(callback, error)
    c_cb_data = _ffi.new_handle(cb_data)

    _handle_error(
        _lib.clingo_ast_parse_files(
            [_ffi.new("char[]", f.encode()) for f in files],
            len(files),
            _lib.pyclingo_ast_callback,
            c_cb_data,
            c_control,
            c_logger,
            c_logger_data,
            message_limit,
        ),
        cb_data,
    )


def parse_string(
    program: str,
    callback: Callable[[AST], None],
    control: Optional[Control] = None,
    logger: Optional[Logger] = None,
    message_limit: int = 20,
) -> None:
    """
    Parse the given program and return an abstract syntax tree for each
    statement via a callback.

    The optional control object can be added to enable parsing of files in
    ASPIF format. The ground statements will be added to the control object.

    Parameters
    ----------
    program
        String representation of the program.
    callback
        Callable taking an ast as argument.
    control
        Control object to add ground rules to.
    logger
        Function to intercept messages normally printed to standard error.
    message_limit
        The maximum number of messages passed to the logger.

    See Also
    --------
    ProgramBuilder
    """
    c_control = control._rep if control is not None else _ffi.NULL

    if logger is not None:
        c_logger_data = _ffi.new_handle(logger)
        c_logger = _lib.pyclingo_logger_callback
    else:
        c_logger_data = _ffi.NULL
        c_logger = _ffi.NULL

    error = _Error()
    cb_data = _CBData(callback, error)
    c_cb_data = _ffi.new_handle(cb_data)

    _handle_error(
        _lib.clingo_ast_parse_string(
            program.encode(),
            _lib.pyclingo_ast_callback,
            c_cb_data,
            c_control,
            c_logger,
            c_logger_data,
            message_limit,
        ),
        cb_data,
    )


class ProgramBuilder(ContextManager["ProgramBuilder"]):
    """
    Object to build non-ground programs.

    Parameters
    ----------
    control
        The `clingo.control.Control` object to attach the builder to.

    See Also
    --------
    parse_string, parse_files

    Notes
    -----
    This class is a context manager and must be used with Python's `with`
    statement.
    """

    def __init__(self, control: Control):
        self._rep = _c_call(
            "clingo_program_builder_t*", _lib.clingo_program_builder_init, control._rep
        )

    def __enter__(self):
        _handle_error(_lib.clingo_program_builder_begin(self._rep))
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        _handle_error(_lib.clingo_program_builder_end(self._rep))
        return False

    def add(self, statement: AST) -> None:
        """
        Adds a statement in form of an `AST` node to the program.

        Parameters
        ----------
        statement
            The statement to add.
        """
        _handle_error(_lib.clingo_program_builder_add(self._rep, statement._rep))


class Transformer:
    """
    Utility class to transform ASTs.

    Classes should inherit from this class and implement functions with name
    `visit_<ast_type>` where `<ast_type>` is the type of the ASTs to visit and
    modify. Such a function should return an updated AST or the same AST if no
    change is necessary. The transformer will take care to copy all parent ASTs
    involving a modified child. Note that the class works like a visitor if
    only self references are returned from such functions.

    Any extra arguments passed to the visit method are passed on to child ASTs.
    """

    def visit(self, ast: AST, *args: Any, **kwargs: Any) -> AST:
        """
        Dispatch to a visit method in a base class or visit and transform the
        children of the given AST if it is missing.
        """
        attr = "visit_" + str(ast.ast_type).replace("ASTType.", "")
        if hasattr(self, attr):
            return getattr(self, attr)(ast, *args, **kwargs)
        return ast.update(**self.visit_children(ast, *args, **kwargs))

    def visit_children(
        self, ast: AST, *args: Any, **kwargs: Any
    ) -> Dict[str, ASTUpdate]:
        """
        Visit and transform the children of the given AST.

        Returns
        -------
        The functions returns a dictionary that can be passed to `AST.update`.
        It contains the attributes and values that have been transformed.
        """
        update: Dict[str, ASTUpdate] = dict()
        for key in ast.child_keys:
            old = getattr(ast, key)
            new = self._dispatch(old, *args, **kwargs)
            if new is not old:
                update[key] = new
        return update

    def visit_sequence(
        self, sequence: ASTSequence, *args: Any, **kwargs: Any
    ) -> MutableSequence[AST]:
        """
        Transform a sequence of ASTs returning the same sequnce if there are no
        changes or a list of ASTs otherwise.
        """
        ret: MutableSequence[AST]
        ret, lst = sequence, []
        for old in sequence:
            lst.append(self(old, *args, **kwargs))
            if lst[-1] is not old:
                ret = lst
        return ret

    def _dispatch(
        self, ast: Union[None, AST, ASTSequence], *args: Any, **kwargs: Any
    ) -> Union[None, AST, MutableSequence[AST]]:
        """
        Visit and transform an (optional) AST or a sequence of ASTs.
        """
        if ast is None:
            return ast

        if isinstance(ast, AST):
            return self.visit(ast, *args, **kwargs)  # type: ignore

        if isinstance(ast, abc.Sequence):
            return self.visit_sequence(ast, *args, **kwargs)

        raise TypeError("unexpected type")

    def __call__(self, ast: AST, *args: Any, **kwargs: Any) -> AST:
        """
        Alternative way to call `Transformer.visit`.
        """
        return self.visit(ast, *args, **kwargs)


def Id(location: Location, name: str) -> AST:
    """
    Construct an AST node of type `ASTType.Id`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_id,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
        )
    )
    return AST(p_ast[0])


def Variable(location: Location, name: str) -> AST:
    """
    Construct an AST node of type `ASTType.Variable`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_variable,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
        )
    )
    return AST(p_ast[0])


def SymbolicTerm(location: Location, symbol: Symbol) -> AST:
    """
    Construct an AST node of type `ASTType.SymbolicTerm`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_symbolic_term,
            p_ast,
            c_location[0],
            _ffi.cast("clingo_symbol_t", symbol._rep),
        )
    )
    return AST(p_ast[0])


def UnaryOperation(location: Location, operator_type: int, argument: AST) -> AST:
    """
    Construct an AST node of type `ASTType.UnaryOperation`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_unary_operation,
            p_ast,
            c_location[0],
            _ffi.cast("int", operator_type),
            argument._rep,
        )
    )
    return AST(p_ast[0])


def BinaryOperation(
    location: Location, operator_type: int, left: AST, right: AST
) -> AST:
    """
    Construct an AST node of type `ASTType.BinaryOperation`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_binary_operation,
            p_ast,
            c_location[0],
            _ffi.cast("int", operator_type),
            left._rep,
            right._rep,
        )
    )
    return AST(p_ast[0])


def Interval(location: Location, left: AST, right: AST) -> AST:
    """
    Construct an AST node of type `ASTType.Interval`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_interval, p_ast, c_location[0], left._rep, right._rep
        )
    )
    return AST(p_ast[0])


def Function(
    location: Location, name: str, arguments: Sequence[AST], external: int
) -> AST:
    """
    Construct an AST node of type `ASTType.Function`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_function,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in arguments]),
            _ffi.cast("size_t", len(arguments)),
            _ffi.cast("int", external),
        )
    )
    return AST(p_ast[0])


def Pool(location: Location, arguments: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Pool`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_pool,
            p_ast,
            c_location[0],
            _ffi.new("clingo_ast_t*[]", [x._rep for x in arguments]),
            _ffi.cast("size_t", len(arguments)),
        )
    )
    return AST(p_ast[0])


def BooleanConstant(value: int) -> AST:
    """
    Construct an AST node of type `ASTType.BooleanConstant`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_boolean_constant, p_ast, _ffi.cast("int", value)
        )
    )
    return AST(p_ast[0])


def SymbolicAtom(symbol: AST) -> AST:
    """
    Construct an AST node of type `ASTType.SymbolicAtom`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(_lib.clingo_ast_type_symbolic_atom, p_ast, symbol._rep)
    )
    return AST(p_ast[0])


def Comparison(term: AST, guards: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Comparison`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_comparison,
            p_ast,
            term._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in guards]),
            _ffi.cast("size_t", len(guards)),
        )
    )
    return AST(p_ast[0])


def Guard(comparison: int, term: AST) -> AST:
    """
    Construct an AST node of type `ASTType.Guard`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_guard, p_ast, _ffi.cast("int", comparison), term._rep
        )
    )
    return AST(p_ast[0])


def ConditionalLiteral(
    location: Location, literal: AST, condition: Sequence[AST]
) -> AST:
    """
    Construct an AST node of type `ASTType.ConditionalLiteral`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_conditional_literal,
            p_ast,
            c_location[0],
            literal._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in condition]),
            _ffi.cast("size_t", len(condition)),
        )
    )
    return AST(p_ast[0])


def Aggregate(
    location: Location,
    left_guard: Optional[AST],
    elements: Sequence[AST],
    right_guard: Optional[AST],
) -> AST:
    """
    Construct an AST node of type `ASTType.Aggregate`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_aggregate,
            p_ast,
            c_location[0],
            _ffi.NULL if left_guard is None else left_guard._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
            _ffi.NULL if right_guard is None else right_guard._rep,
        )
    )
    return AST(p_ast[0])


def BodyAggregateElement(terms: Sequence[AST], condition: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.BodyAggregateElement`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_body_aggregate_element,
            p_ast,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in condition]),
            _ffi.cast("size_t", len(condition)),
        )
    )
    return AST(p_ast[0])


def BodyAggregate(
    location: Location,
    left_guard: Optional[AST],
    function: int,
    elements: Sequence[AST],
    right_guard: Optional[AST],
) -> AST:
    """
    Construct an AST node of type `ASTType.BodyAggregate`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_body_aggregate,
            p_ast,
            c_location[0],
            _ffi.NULL if left_guard is None else left_guard._rep,
            _ffi.cast("int", function),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
            _ffi.NULL if right_guard is None else right_guard._rep,
        )
    )
    return AST(p_ast[0])


def HeadAggregateElement(terms: Sequence[AST], condition: AST) -> AST:
    """
    Construct an AST node of type `ASTType.HeadAggregateElement`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_head_aggregate_element,
            p_ast,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
            condition._rep,
        )
    )
    return AST(p_ast[0])


def HeadAggregate(
    location: Location,
    left_guard: Optional[AST],
    function: int,
    elements: Sequence[AST],
    right_guard: Optional[AST],
) -> AST:
    """
    Construct an AST node of type `ASTType.HeadAggregate`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_head_aggregate,
            p_ast,
            c_location[0],
            _ffi.NULL if left_guard is None else left_guard._rep,
            _ffi.cast("int", function),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
            _ffi.NULL if right_guard is None else right_guard._rep,
        )
    )
    return AST(p_ast[0])


def Disjunction(location: Location, elements: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Disjunction`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_disjunction,
            p_ast,
            c_location[0],
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
        )
    )
    return AST(p_ast[0])


def TheorySequence(location: Location, sequence_type: int, terms: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.TheorySequence`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_sequence,
            p_ast,
            c_location[0],
            _ffi.cast("int", sequence_type),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
        )
    )
    return AST(p_ast[0])


def TheoryFunction(location: Location, name: str, arguments: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryFunction`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_function,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in arguments]),
            _ffi.cast("size_t", len(arguments)),
        )
    )
    return AST(p_ast[0])


def TheoryUnparsedTermElement(operators: Sequence[str], term: AST) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryUnparsedTermElement`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_operators = [_ffi.new("char[]", x.encode()) for x in operators]
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_unparsed_term_element,
            p_ast,
            _ffi.new("char*[]", c_operators),
            _ffi.cast("size_t", len(operators)),
            term._rep,
        )
    )
    return AST(p_ast[0])


def TheoryUnparsedTerm(location: Location, elements: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryUnparsedTerm`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_unparsed_term,
            p_ast,
            c_location[0],
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
        )
    )
    return AST(p_ast[0])


def TheoryGuard(operator_name: str, term: AST) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryGuard`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_guard,
            p_ast,
            _ffi.new("char const[]", operator_name.encode()),
            term._rep,
        )
    )
    return AST(p_ast[0])


def TheoryAtomElement(terms: Sequence[AST], condition: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryAtomElement`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_atom_element,
            p_ast,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in condition]),
            _ffi.cast("size_t", len(condition)),
        )
    )
    return AST(p_ast[0])


def TheoryAtom(
    location: Location, term: AST, elements: Sequence[AST], guard: Optional[AST]
) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryAtom`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_atom,
            p_ast,
            c_location[0],
            term._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in elements]),
            _ffi.cast("size_t", len(elements)),
            _ffi.NULL if guard is None else guard._rep,
        )
    )
    return AST(p_ast[0])


def Literal(location: Location, sign: int, atom: AST) -> AST:
    """
    Construct an AST node of type `ASTType.Literal`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_literal,
            p_ast,
            c_location[0],
            _ffi.cast("int", sign),
            atom._rep,
        )
    )
    return AST(p_ast[0])


def TheoryOperatorDefinition(
    location: Location, name: str, priority: int, operator_type: int
) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryOperatorDefinition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_operator_definition,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.cast("int", priority),
            _ffi.cast("int", operator_type),
        )
    )
    return AST(p_ast[0])


def TheoryTermDefinition(
    location: Location, name: str, operators: Sequence[AST]
) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryTermDefinition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_term_definition,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in operators]),
            _ffi.cast("size_t", len(operators)),
        )
    )
    return AST(p_ast[0])


def TheoryGuardDefinition(operators: Sequence[str], term: str) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryGuardDefinition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_operators = [_ffi.new("char[]", x.encode()) for x in operators]
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_guard_definition,
            p_ast,
            _ffi.new("char*[]", c_operators),
            _ffi.cast("size_t", len(operators)),
            _ffi.new("char const[]", term.encode()),
        )
    )
    return AST(p_ast[0])


def TheoryAtomDefinition(
    location: Location,
    atom_type: int,
    name: str,
    arity: int,
    term: str,
    guard: Optional[AST],
) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryAtomDefinition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_atom_definition,
            p_ast,
            c_location[0],
            _ffi.cast("int", atom_type),
            _ffi.new("char const[]", name.encode()),
            _ffi.cast("int", arity),
            _ffi.new("char const[]", term.encode()),
            _ffi.NULL if guard is None else guard._rep,
        )
    )
    return AST(p_ast[0])


def Rule(location: Location, head: AST, body: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Rule`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_rule,
            p_ast,
            c_location[0],
            head._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
        )
    )
    return AST(p_ast[0])


def Definition(location: Location, name: str, value: AST, is_default: int) -> AST:
    """
    Construct an AST node of type `ASTType.Definition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_definition,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            value._rep,
            _ffi.cast("int", is_default),
        )
    )
    return AST(p_ast[0])


def ShowSignature(location: Location, name: str, arity: int, positive: int) -> AST:
    """
    Construct an AST node of type `ASTType.ShowSignature`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_show_signature,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.cast("int", arity),
            _ffi.cast("int", positive),
        )
    )
    return AST(p_ast[0])


def ShowTerm(location: Location, term: AST, body: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.ShowTerm`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_show_term,
            p_ast,
            c_location[0],
            term._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
        )
    )
    return AST(p_ast[0])


def Minimize(
    location: Location,
    weight: AST,
    priority: AST,
    terms: Sequence[AST],
    body: Sequence[AST],
) -> AST:
    """
    Construct an AST node of type `ASTType.Minimize`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_minimize,
            p_ast,
            c_location[0],
            weight._rep,
            priority._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
        )
    )
    return AST(p_ast[0])


def Script(location: Location, name: str, code: str) -> AST:
    """
    Construct an AST node of type `ASTType.Script`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_script,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("char const[]", code.encode()),
        )
    )
    return AST(p_ast[0])


def Program(location: Location, name: str, parameters: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Program`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_program,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in parameters]),
            _ffi.cast("size_t", len(parameters)),
        )
    )
    return AST(p_ast[0])


def External(
    location: Location, atom: AST, body: Sequence[AST], external_type: AST
) -> AST:
    """
    Construct an AST node of type `ASTType.External`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_external,
            p_ast,
            c_location[0],
            atom._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
            external_type._rep,
        )
    )
    return AST(p_ast[0])


def Edge(location: Location, node_u: AST, node_v: AST, body: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.Edge`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_edge,
            p_ast,
            c_location[0],
            node_u._rep,
            node_v._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
        )
    )
    return AST(p_ast[0])


def Heuristic(
    location: Location,
    atom: AST,
    body: Sequence[AST],
    bias: AST,
    priority: AST,
    modifier: AST,
) -> AST:
    """
    Construct an AST node of type `ASTType.Heuristic`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_heuristic,
            p_ast,
            c_location[0],
            atom._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
            bias._rep,
            priority._rep,
            modifier._rep,
        )
    )
    return AST(p_ast[0])


def ProjectAtom(location: Location, atom: AST, body: Sequence[AST]) -> AST:
    """
    Construct an AST node of type `ASTType.ProjectAtom`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_project_atom,
            p_ast,
            c_location[0],
            atom._rep,
            _ffi.new("clingo_ast_t*[]", [x._rep for x in body]),
            _ffi.cast("size_t", len(body)),
        )
    )
    return AST(p_ast[0])


def ProjectSignature(location: Location, name: str, arity: int, positive: int) -> AST:
    """
    Construct an AST node of type `ASTType.ProjectSignature`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_project_signature,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.cast("int", arity),
            _ffi.cast("int", positive),
        )
    )
    return AST(p_ast[0])


def Defined(location: Location, name: str, arity: int, positive: int) -> AST:
    """
    Construct an AST node of type `ASTType.Defined`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_defined,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.cast("int", arity),
            _ffi.cast("int", positive),
        )
    )
    return AST(p_ast[0])


def TheoryDefinition(
    location: Location, name: str, terms: Sequence[AST], atoms: Sequence[AST]
) -> AST:
    """
    Construct an AST node of type `ASTType.TheoryDefinition`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_theory_definition,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", name.encode()),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in terms]),
            _ffi.cast("size_t", len(terms)),
            _ffi.new("clingo_ast_t*[]", [x._rep for x in atoms]),
            _ffi.cast("size_t", len(atoms)),
        )
    )
    return AST(p_ast[0])


def Comment(location: Location, value: str, comment_type: int) -> AST:
    """
    Construct an AST node of type `ASTType.Comment`.
    """
    p_ast = _ffi.new("clingo_ast_t**")
    c_location = _c_location(location)
    _handle_error(
        _lib.clingo_ast_build(
            _lib.clingo_ast_type_comment,
            p_ast,
            c_location[0],
            _ffi.new("char const[]", value.encode()),
            _ffi.cast("int", comment_type),
        )
    )
    return AST(p_ast[0])
