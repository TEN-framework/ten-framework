%
% * option = 1: ht models
% * option = 2: ht models minimizing h
% * option = 3: equilibrium models
%
#const option=1.

% define atoms
atom( A ) :- atom_tuple(_,A).
atom(|L|) :-          literal_tuple(_,L).
atom(|L|) :- weighted_literal_tuple(_,L).

% h and t are classical models s.t. h \subseteq t

model(h). model(t).

{ hold(A,h) } :- atom(A),    option = 1.
{ hold(A,t) } :- atom(A).
:- hold(L,h), not hold(L,t).

% option = 2 simply does not open hold(L,h)

% option = 3

:- not hold(L,h), hold(L,t), option = 3.


%
% t is a classical model, and
% h is a classical model of the reduct wrt. t
%

conjunction(B,M) :- model(M), literal_tuple(B),
        hold(L,M) : literal_tuple(B, L), L > 0;
    not hold(L,t) : literal_tuple(B,-L), L > 0.

body(normal(B),M) :- rule(_,normal(B)), conjunction(B,M).
body(sum(B,G),M)  :- model(M), rule(_,sum(B,G)),
    #sum { W,L :     hold(L,M), weighted_literal_tuple(B, L,W), L > 0 ;
           W,L : not hold(L,t), weighted_literal_tuple(B,-L,W), L > 0 } >= G.

               hold(A,M) :  atom_tuple(H,A)   :- rule(disjunction(H),B), body(B,M).
hold(A,M); not hold(A,t) :- atom_tuple(H,A),     rule(     choice(H),B), body(B,M).

#show.
#show (T,M) : output(T,B), conjunction(B,M).
