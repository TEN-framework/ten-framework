'''
Module providing functions and classes to control the grounding and solving
process.

Next follow some concepts used throughout the modules of the clingo package.

Terms
-----
Terms without variables and interpreted functions are called symbols in the
following. They are wrapped in the `clingo.symbol.Symbol` class.

Symbolic Atoms and Literals
---------------------------
*Symbolic atoms* without variables and interpreted functions, which appear in
ground logic programs, are captured using the `clingo.symbol.Symbol` class.
They must be of type `clingo.symbol.SymbolType.Function`. Furthermore, some
functions accept *symbolic literals*, which are represented as pairs of symbols
and Booleans. The Boolean stands for the sign of the literal (`True` for
positive and `False` for negative).

Program Literals
----------------
When passing a ground logic program to a solver, clingo does not use a human
readable textual representation but the aspif format. The literals in this
format are called *program literals*. They are non-zero integers associated
with symbolic atoms, theory atoms, and also without any association if they are
used to translate complex language constructs not directly representable in
aspif format. The sign of a program literal is used to represent default
negation. Symbolic atoms can be mapped to program literals using the
`clingo.symbolic_atoms` module and theory atoms using the `clingo.theory_atoms`
module. Note that symbolic and theory atoms can share the same program
literals. Finally, the `clingo.backend` module can also be used to introduce
fresh symbolic atoms and program literals.

Solver Literals
---------------
Before solving, programs in aspif format are translated to an internal solver
representation, where program literals are again mapped to non-zero integers,
so called *solver literals*. The `clingo.propagator.PropagateInit.solver_literal`
function can be used to map program literals to solver literals. Note that
different program literals can share the same solver literal.

Embedded Python Code
--------------------
If the clingo application is build with Python support, clingo will also be
able to execute Python code embedded in logic programs. Functions defined in a
Python script block are callable during the instantiation process using
`@`-syntax. The default grounding/solving process can be customized if a main
function is provided.

## Examples

The first example shows how to use the clingo module from Python.

    >>> from clingo.symbol import Number
    >>> from clingo.control import Control
    >>>
    >>> class Context:
    ...     def inc(self, x):
    ...         return Number(x.number + 1)
    ...     def seq(self, x, y):
    ...         return [x, y]
    ...
    >>> def on_model(m):
    ...     print (m)
    ...
    >>> ctl = Control()
    >>> ctl.add("base", [], """\\
    ... p(@inc(10)).
    ... q(@seq(1,2)).
    ... """)
    >>> ctl.ground([("base", [])], context=Context())
    >>> print(ctl.solve(on_model=on_model))
    p(11) q(1) q(2)
    SAT

The second example shows how to use Python code from clingo.

    #script (python)

    from clingo.symbol import Number

    class Context:
        def inc(self, x):
            return Number(x.number + 1)

        def seq(self, x, y):
            return [x, y]

    def main(prg):
        prg.ground([("base", [])], context=Context())
        prg.solve()

    #end.

    p(@inc(10)).
    q(@seq(1,2)).
'''

from .core import *
from .symbol import *
from .symbolic_atoms import *
from .theory_atoms import *
from .solving import *
from .propagator import *
from .backend import *
from .configuration import *
from .statistics import *
from .control import *
from .application import *

__version__ = ".".join(map(str, version()))
