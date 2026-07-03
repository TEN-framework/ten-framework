#program object(m,k).
#external assigned(m,1).

vertex(m).
:- not assigned(m,1).

next(m,C,k) :- color(C).

next(m,N,C,k) :- edge(m,N), vertex(N), color(C).
next(N,m,C,k) :- edge(N,m), vertex(N), color(C).

#program horizon(c,k).

color(c).

next(M,c,k) :- vertex(M).

next(M,N,c,k) :- edge(M,N), vertex(M), vertex(N).

#program expand(k).
#external assigned(M,C+1) : next(M,C,k).

{ assign(M,C) } :- next(M,C,k).

assigned(M,C) :- assign(M,C), next(M,C,k).
assigned(M,C) :- assigned(M,C+1), next(M,C,k).
:- assign(M,C-1), assigned(M,C), next(M,C,k).

:- assign(M,C), assign(N,C), next(M,N,C,k).

#show assign/2.
