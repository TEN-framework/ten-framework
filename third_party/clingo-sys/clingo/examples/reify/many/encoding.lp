model(1..m).

conjunction(B,M) :- model(M), literal_tuple(B),
        hold(L,M) : literal_tuple(B, L), L > 0;
    not hold(L,M) : literal_tuple(B,-L), L > 0.

body(normal(B),M) :- rule(_,normal(B)), conjunction(B,M).
body(sum(B,G),M)  :- model(M), rule(_,sum(B,G)),
    #sum { W,L :     hold(L,M), weighted_literal_tuple(B, L,W), L > 0 ;
           W,L : not hold(L,M), weighted_literal_tuple(B,-L,W), L > 0 } >= G.

  hold(A,M) : atom_tuple(H,A)   :- rule(disjunction(H),B), body(B,M).
{ hold(A,M) : atom_tuple(H,A) } :- rule(     choice(H),B), body(B,M).

show(T,M) :- output(T,B), conjunction(B,M).
#show.
#show (T,M) : show(T,M).

#const k=1.

:- model(M), model(N), M<N, option = 1,
    #sum{ 1,T: show(T,M), not show(T,N) ;
          1,T: not show(T,M), show(T,N) } < k.

#maximize{ 1,T,M,N: show(T,M), not show(T,N), model(N), option = 2 }.
