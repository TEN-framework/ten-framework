edge(a,(b;c)).
edge(b,(c;d;e)).
edge(d,f).

edge(x,(a;c)). % edge to keep something to solve

edge(X,Y) :- edge(Y,X).
vertex(X) :- edge(X,Y;Y,X).
