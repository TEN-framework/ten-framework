#include "../common/meta.lp".

atom( A ) :- atom_tuple(_,A).
atom(|L|) :-          literal_tuple(_,L).
atom(|L|) :- weighted_literal_tuple(_,L).

{ hold(A) : atom(A) }.
