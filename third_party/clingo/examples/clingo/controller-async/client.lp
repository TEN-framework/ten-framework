#const m = 10.
#const n = 190.

#program pigeon.
#external p.

1 { p(X); q(X) } 1 :- X = 1..n, p.
:- not n+1 { p(1..n); q(1..n) }, p.

#program sleep(k).
#external sleep(k).

1 { p(k,X); q(k,X) } 1 :- X = 1..m, sleep(k), not p.
:- not m+1 { p(k,1..m); q(k,1..m) }, sleep(k), not p.

