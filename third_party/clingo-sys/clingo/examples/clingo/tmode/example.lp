#program initial.

q(0).

#program dynamic.

q(X+1) :- q'(X).
r(X) :- r'(X).

#program static.

r(X) :- q(X).

#program final.

:- #sum { X : r(X) } < 10.
