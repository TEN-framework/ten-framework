% NOTE: assumes that a rule has no more than one head

sum(B,G,T) :- rule(_,sum(B,G)), T = #sum { W,L : weighted_literal_tuple(B,L,W) }.

% extract supports of atoms and facts

supp(A,B) :- rule(     choice(H),B), atom_tuple(H,A).
supp(A,B) :- rule(disjunction(H),B), atom_tuple(H,A).

supp(A) :- supp(A,_).

atom(|L|) :- weighted_literal_tuple(_,L,_).
atom(|L|) :- literal_tuple(_,L).
atom( A ) :- atom_tuple(_,A).

fact(A) :- rule(disjunction(H),normal(B)), atom_tuple(H,A), not literal_tuple(B,_).

% generate interpretation

true(atom(A))                :- fact(A).
true(atom(A)); fail(atom(A)) :- supp(A), not fact(A).
               fail(atom(A)) :- atom(A), not supp(A).

true(normal(B)) :- literal_tuple(B),
    true(atom(L)) : literal_tuple(B, L), L > 0;
    fail(atom(L)) : literal_tuple(B,-L), L > 0.
fail(normal(B)) :- literal_tuple(B, L), fail(atom(L)), L > 0.
fail(normal(B)) :- literal_tuple(B,-L), true(atom(L)), L > 0.

true(sum(B,G)) :- sum(B,G,T),
    #sum { W,L : true(atom(L)), weighted_literal_tuple(B, L,W), L > 0 ;
           W,L : fail(atom(L)), weighted_literal_tuple(B,-L,W), L > 0 } >= G.
fail(sum(B,G)) :- sum(B,G,T),
    #sum { W,L : fail(atom(L)), weighted_literal_tuple(B, L,W), L > 0 ;
           W,L : true(atom(L)), weighted_literal_tuple(B,-L,W), L > 0 } >= T-G+1.

% verify supported model properties

bot :- rule(disjunction(H),B), true(B), fail(atom(A)) : atom_tuple(H,A).
bot :- true(atom(A)), fail(B) : supp(A,B).

% verify acyclic derivability

internal(C,normal(B)) :- scc(C,A), supp(A,normal(B)), scc(C,A'), literal_tuple(B,A').
internal(C,sum(B,G))  :- scc(C,A), supp(A,sum(B,G)),  scc(C,A'), weighted_literal_tuple(B,A',W).

external(C,normal(B)) :- scc(C,A), supp(A,normal(B)), not internal(C,normal(B)).
external(C,sum(B,G))  :- scc(C,A), supp(A,sum(B,G)),  not internal(C,sum(B,G)).

steps(C,Z-1) :- scc(C,_), Z = { scc(C,A) : not fact(A) }.

wait(C,atom(A),0)   :- scc(C,A), fail(B) : external(C,B), supp(A,B).
wait(C,normal(B),I) :- internal(C,normal(B)), fail(normal(B)), steps(C,Z), I = 0..Z-1.
wait(C,normal(B),I) :- internal(C,normal(B)), literal_tuple(B,A), wait(C,atom(A),I), steps(C,Z), I < Z.
wait(C,sum(B,G),I)  :- internal(C,sum(B,G)), steps(C,Z), I = 0..Z-1, sum(B,G,T),
    #sum { W,L :   fail(atom(L)),   weighted_literal_tuple(B, L,W), L > 0, not scc(C,L) ;
           W,L : wait(C,atom(L),I), weighted_literal_tuple(B, L,W), L > 0,     scc(C,L) ;
           W,L :   true(atom(L)),   weighted_literal_tuple(B,-L,W), L > 0               } >= T-G+1.
wait(C,atom(A),I)   :- wait(C,atom(A),0), steps(C,Z), I = 1..Z, wait(C,B,I-1) : supp(A,B), internal(C,B).

bot :- scc(C,A), true(atom(A)), wait(C,atom(A),Z), steps(C,Z).

% saturate interpretations that are not answer sets

true(atom(A)) :- supp(A), not fact(A), bot.
fail(atom(A)) :- supp(A), not fact(A), bot.

