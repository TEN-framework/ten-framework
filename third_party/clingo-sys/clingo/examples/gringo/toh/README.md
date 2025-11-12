# Solving the Towers of Hanoi Problem

This example shows how to incrementally solve the towers of hanoi problem using
clingo's inbuild incremental solving mode.

Note that, with a fixed bound, the problem can also be grounded by gringo and
then solved by clasp.

## Example Calls

    clingo tohI.lp tohE.lp
    gringo -c imax=16 tohE.lp tohI.lp | clasp
