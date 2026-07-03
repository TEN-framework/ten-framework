1 { cycle(T,U) : task(U), U != T } 1 :- task(T).
1 { cycle(T,U) : task(T), U != T } 1 :- task(U).

reach(M) :- M = #min { T : task(T) }.
reach(U) :- reach(T), cycle(T,U).
:- task(T), not reach(T).

1 { start(T) : task(T) } 1.

permutation(T,U) :- cycle(T,U), not start(U).

seq((T,M),(T,M+1),D) :- task(T), duration(T,M,D), machine(M+1).
seq((T1,M),(T2,M),D) :- permutation(T1,T2), duration(T1,M,D).

&diff { T1-T2 } <= -D :- seq(T1,T2,D).
&diff { 0-(T,M) } <= 0 :- duration(T,M,D).
&diff { (T,M)-bound } <= -D :- duration(T,M,D).

#show permutation/2.
