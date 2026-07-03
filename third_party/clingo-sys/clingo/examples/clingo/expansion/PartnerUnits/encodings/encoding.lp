#program base.

stat(T,1,1) :- object(T,1).

#program object(x,k).
#external assigned(T,x,1) : item(T,x).

item(T,x) :- object(T,x).
:- item(T,x), not assigned(T,x,1).

stat(T,x,I+1) :- item(T,x), follow(T,X,T,x), stat(T,X,I).

next(T,x,U,k) :- item(T,x), unit(U), U <= I : stat(T,x,I).

#program horizon(u,k).

unit(u).
pair(1..u-1,u).

next(u,k).
next(T,X,u,k) :- item(T,X), u <= I : stat(T,X,I).

conn(u-1,u-2,u) :- 2 < u.
conn(u,U,U+1)   :- unit(U), U+1 < u.
conn(U,u-1,u)   :- unit(U), U < u-1.

prop(U,u-M,M,k) :- unit(U), maxPU(M), U < u-M.
prop(U,u-N,M,k) :- unit(U), maxPU(M), N = M+1, u-M <= U, N < u.
prop(u,U-1,M,k) :- prop(u,U,M,k), maxPU(M), 1 < U.
prop(U,W,M-1,k) :- conn(U,V,W), prop(U,V,M,k), 1 < M.

#program expand(k).
#external assigned(T,X,U+1) : next(T,X,U,k), stat(T,X,I), U < I.
#external assigned(T,X,U+1) : next(T,X,U,k), not stat(T,1,1).
#external partner(U,V,k+1) : pair(U,V).

{ assign(T,X,U) } :- next(T,X,U,k).

assigned(T,X,U) :- assign(T,X,U), next(T,X,U,k).
assigned(T,X,U) :- assigned(T,X,U+1), next(T,X,U,k).
:- assign(T,X,U-1), assigned(T,X,U), next(T,X,U,k).

left(T,Y,U,2) :- follow(T,X,T,Y), follow(T,Y,T,Z), assign(T,X,U), next(T,Z,U,k).
left(T,Y,U,2) :- follow(T,X,T,Y), follow(T,Y,T,Z), left(T,X,U,2), next(T,Z,U,k).
left(T,Y,U,1) :- follow(T,X,T,Y), left(T,X,U,2), assign(T,X,U), next(T,Y,U,k).
left(T,Y,U,1) :- follow(T,X,T,Y), left(T,X,U,1), next(T,Y,U,k).
:- left(T,X,U,1), assign(T,X,U), next(T,X,U,k).

part(-T,Y,U) :- relate(T,X,-T,Y), assign(T,X,U), next(-T,Y,1,k).
part(-T,Y,U) :- relate(T,X,-T,Y), assign(T,X,U), next(U,k), item(-T,Y), X < Y.

partner(U,V,k) :- partner(U,V,k+1), not next(V,k).
partner(U,V,k) :- part(T,X,U), assign(T,X,V), pair(U,V), next(T,X,1,k).
partner(U,V,k) :- assign(T,X,U), part(T,X,V), pair(U,V), next(T,X,1,k).

partner(U,V) :- partner(U,V,k+1), next(V,k).
partner(U,V) :- part(T,X,U), assign(T,X,V), pair(U,V), next(V,k).
partner(U,V) :- assign(T,X,U), part(T,X,V), pair(U,V), next(V,k).
partner(V,U) :- partner(U,V), next(V,k).

prop(U,W,M)   :- conn(U,V,W), partner(U,V), prop(U,V,M,k), maxPU(M).
prop(U,W,M-1) :- conn(U,V,W), partner(U,V), prop(U,V,M-1,k), prop(U,V,M).
prop(U,W,M)   :- conn(U,V,W), prop(U,V,M), prop(U,V,M,k).
:- conn(U,V,W), prop(U,W,1), partner(U,W), prop(U,V,1,k).

#show assign/3.
