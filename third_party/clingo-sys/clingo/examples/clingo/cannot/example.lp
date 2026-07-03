#show in/2.

{in(X,Y)} :- edge(X,Y).

&cannot {"pick outgoing edges %->% and %->%",X,Y,X,Z}
:- in(X,Y), in(X,Z), Y!=Z.
&cannot {"pick incoming edges %->% and %->%",X,Z,Y,Z}
:- in(X,Z), in(Y,Z), X!=Y.

reached(X) :- in(1,X).
reached(Y) :- reached(X), in(X,Y).

&cannot {"reach vertex % from vertex 1",X}
:- vtx(X), not reached(X).

vtx(1). vtx(2). vtx(3). vtx(4). vtx(5).
edge(1,2). edge(2,3). edge(2,4).
edge(3,1). edge(3,4). edge(4,3). edge(4,1).
