p(1).
p(a).
p(b).
p(s(f)).

gather(1).
gather(@gather(X)) :- p(X).

enumerate(X,N) :- gather(1), (X,N) = @sort().
