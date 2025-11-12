#const n = 10.

num(1).
num(X+1) :- num(X), X < n.

queen(X,Y) :- not free(X,Y), num(X), num(Y).
free(X,Y) :- not queen(X,Y), num(X), num(Y).

row(X) :- queen(X,Y).
col(Y) :- queen(X,Y).

fail :- num(X), not row(X), not fail.
fail :- num(Y), not col(Y), not fail.

fail :- queen(X,Y), queen(X,V), Y < V, not fail.
fail :- queen(X,Y), queen(U,Y), X < U, not fail.

fail :- queen(X,Y), queen(U,V), X-Y == U-V, f(X,Y) < f(U,V), not fail.
fail :- queen(X,Y), queen(U,V), X+Y == U+V, f(X,Y) < f(U,V), not fail.
