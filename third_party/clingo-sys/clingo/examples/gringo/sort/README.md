# Sorting Terms

Sorting a set of terms in ASP is quite difficult to implement efficiently.
Nicely readable programs like

    next(X,Z) :- p(X), #false : X<Y, p(Y), Y<Z ; p(Z), X<Z.

or

    next(X,Y) :- p(X), Y = #min { Z : p(Z), Z > X }, Y != #sup.

do not scale well enough for large input (qubic or quadratic for the programs
above).

In practice it might perform best to already sort the terms in the instance. Or
the scripting API can be used to obtain better performing code.

Note that in the example there are no next predicates but an enumerate
predicate, which is more flexible in practice. For example, to encode the
above:

    next(X,Y) :- enumerate(N,X), enumerate(N+1,Y).

## Example calls

    gringo --text sort-lua.lp encoding.lp
    gringo --text sort-py.lp encoding.lp
