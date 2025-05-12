conjunction(B) :- literal_tuple(B),
        hold(L) : literal_tuple(B, L), L > 0;
       nhold(L) : literal_tuple(B,-L), L > 0.

body(normal(B)) :- rule(_,normal(B)), conjunction(B).
body(sum(B,G))  :- rule(_,sum(B,G)),
    #sum { W,L :     hold(L), weighted_literal_tuple(B, L,W), L > 0 ;
           W,L :    nhold(L), weighted_literal_tuple(B,-L,W), L > 0 } >= G.

  hold(A) : atom_tuple(H,A)   :- rule(disjunction(H),B), body(B).
{ hold(A) : atom_tuple(H,A) } :- rule(     choice(H),B), body(B).

#show.
#show T : output(T,B), conjunction(B).

% atoms that occur negated
atom(L) :-          literal_tuple(B,-L), L > 0.
atom(L) :- weighted_literal_tuple(B,-L), L > 0.

% open fresh atoms, and constrain their truth value
{ nhold(L) } :- atom(L).
:-     hold(L),     nhold(L), atom(L).
:- not hold(L), not nhold(L), atom(L).
