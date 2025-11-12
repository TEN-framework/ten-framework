% clingo-4.3.0 instance.lp color.ilp -c imin=7 0

#include <incmode>.

#program check(k).
:- query(0), k == 0.

#program step(k).

node(X)   :- node(X,k).
edge(X,Y) :- edge(X,Y,k).
color(C)  :- color(C,k).

new(X,C,k) :- node(X,k), color(C).
new(X,C,k) :- node(X), color(C,k).

new(X,Y,C,k) :- edge(X,Y,k), color(C).
new(X,Y,C,k) :- edge(X,Y), new(X,C,k).
new(X,Y,C,k) :- edge(X,Y), node(Y,k), color(C).

{ assign(X,C) } :- new(X,C,k).

assigned(X,C) :- new(X,C,k), assign(X,C).
assigned(X,C) :- new(X,C,k), assigned(X,C+1).
:- new(X,C,k), assign(X,C-1), assigned(X,C).
:- node(X,k), not assigned(X,1).

:- new(X,Y,C,k), assign(X,C), assign(Y,C). % , edge(X,Y)

#external assigned(X,1)   : node(X,k).
#external assigned(X,C+1) : new(X,C,k).

#show assign/2.
% clingo-4 instance.lp color.lp 0
% clingo-4.3.0 instance.lp color.ilp -c imin=7 0

#program base.

node(1,1).
color(1,2).
color(2,3).
node(2,4).
edge(1,2,5).
node(3,6). edge(2,3,6).
color(3,7).
