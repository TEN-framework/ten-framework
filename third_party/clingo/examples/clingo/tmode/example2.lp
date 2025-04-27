q(0) :- initially.

q(X+1) :- q'(X), not initially.
r(X) :- r'(X), not initially.

r(X) :- q(X).

:- #sum { X : r(X) } < 10, finally.
