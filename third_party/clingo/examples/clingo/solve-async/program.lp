#const n = 17.

1 { p(X); q(X) } 1 :- X = 1..n.
:- not n+1 { p(1..n); q(1..n) }.

