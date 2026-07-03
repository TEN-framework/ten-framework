% mark c cells of an n x n grid
#const n=3.
#const c=3.

% the marked cells should be all together i.e. they all should be reachable from any other
{ x((1..n,1..n)) } = c.

connect(C) :- x(C), C'<=C : x(C').
connect((X',Y')) :- connect((X,Y)), x((X',Y')), |X-X'|+|Y-Y'|=1.

:- x(C), not connect(C).

#show x/1.
