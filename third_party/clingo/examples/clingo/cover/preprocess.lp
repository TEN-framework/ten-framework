#program base.

edge(0,X,Y) :- edge(X,Y).

#program step(k).

:- not leaf(k-1,@fix(k)).

1 { leaf(k,X) : vertex(X) } 1.
cover(k,X) :- edge(k-1,X,Y), leaf(k,Y).
edge(k,X,Y) :- edge(k-1,X,Y), not cover(k,X), not cover(k,Y).

:- edge(k-1,X,Y), leaf(k,Y), edge(k-1,Y,Z), X < Z.
:- leaf(k,Y), not edge(k-1,_,Y).
