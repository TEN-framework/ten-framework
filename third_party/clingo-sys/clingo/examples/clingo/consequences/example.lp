atom(p(1..10);q).
{ holds(X) : atom(X) } 3.
holds(p(X)) :- holds(p(X-1)), atom(p(X)),     holds(q).
holds(p(X)) :- holds(p(X+1)), atom(p(X)), not holds(q).
