vertex(X) :- X = @vertex().
edge(X,Y) :- (X,Y) = @edge().
cover(X)  :- X = @cover().

{ cover(X) : vertex(X) }.
:- edge(X,Y), not cover(X), not cover(Y).
#minimize { 1,X : cover(X) }.

#show cover/1.
