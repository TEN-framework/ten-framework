# 15 Puzzle

This is an example how to solve the [15 puzzle] problem.  It supports two
solving modes: a non-consecutive mode where each tile is moved individually and
a consecutive mode where multiple tiles are moved at once.  In the latter mode,
much harder instances can be solved but the resulting solutions are not
necessarily optimal w.r.t. the number of single tile moves.

## Example Calls

    clingo encoding.lp instance1.lp -c consecutive=1
    clingo encoding.lp instance1.lp -c consecutive=0
    clingo encoding.lp instance2.lp -c consecutive=1 -t8,split --config=crafty

[15 puzzle]: https://en.wikipedia.org/wiki/15_puzzle
