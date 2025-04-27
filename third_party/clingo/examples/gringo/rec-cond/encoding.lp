%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% INNER VERTICES WITHOUT SELF-LOOP ARE REDUCIBLE %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

inner(V) :- edge(_,V), edge(V,_), not edge(V,V).

link(U,V) :- edge(U,V), 1 { inner(U); inner(V) }.

linked(U,V) :- link(U,V).
linked(U,V) :- link(U,W), linked(W,V), inner(W), 1 { inner(U); inner(V) }.

inlink(V,N) :- inner(V), N = { linked(U,V) : U != V }.
oulink(V,N) :- inner(V), N = { linked(V,W) : V != W }.

prefer(V,N) :- inner(V), N = { inner(1..V-1) }.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% ITERATIVELY REDUCE INNER VERTICES AS LONG AS SOME HAS ONE PREDECESSOR/SUCCESSOR %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

unreduced(V,1)   :- inner(V).
unreduced(V,I+1) :- unreduced(V,I), reduced(W,I), V != W.

linked(U,V,1)   :- link(U,V).
linked(U,V,I+1) :- linked(U,W,I), linked(W,V,I), reduced(W,I).
linked(U,V,I+1) :- linked(U,V,I), reduced(I),
         2 { unreduced(U,I+1); unreduced(V,I+1); not inner(U); not inner(V) }.

irreducible(V,I) :- unreduced(V,I), linked(V,V,I).
irreducible(V,I) :- unreduced(V,I),
         2 { linked(U,V,I) : U != V }, 2 { linked(V,W,I) : V != W }.

unlinked(U,V,1)   :- linked(U,V), not link(U,V).
unlinked(U,V,I+1) :- linked(U,V,I), reduced(U,I), inner(V).
unlinked(U,V,I+1) :- linked(U,V,I), reduced(V,I), inner(U).
unlinked(U,V,I+1) :- unlinked(U,V,I), reduced(U,J), J <= I, unreduced(V,I+1).
unlinked(U,V,I+1) :- unlinked(U,V,I), reduced(V,J), J <= I, unreduced(U,I+1).
unlinked(U,V,I+1) :- unlinked(U,V,I), reduced(W,I), U != W, V != W,
           1 { unlinked(U,W,I); unlinked(W,V,I); not linked(U,W); not linked(W,V) }.

reducible(V,I) :- unreduced(V,I), inlink(V,N),
      N-1 { unlinked(U,V,I) : U != V }, 1 { unlinked(V,V,I); not linked(V,V) }.
reducible(V,I) :- unreduced(V,I), oulink(V,N),
      N-1 { unlinked(V,W,I) : V != W }, 1 { unlinked(V,V,I); not linked(V,V) }.

reduced(V,I) :- reducible(V,I), prefer(V,N),
      N { irreducible(1..V-1,I); reduced(W,J) : W < V, J < I }.
reduced(I)   :- reduced(_,I).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% FINAL RESULT OF ITERATIVE REDUCTION %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

outvertex(V) :- vertex(V), not inner(V).
outvertex(V) :- unreduced(V,I), not reduced(I).

outedge(U,V) :- edge(U,V), not link(U,V).
outedge(U,V) :- linked(U,V,I), not reduced(I).

#show outvertex/1.
#show outedge/2.
