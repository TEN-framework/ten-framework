#program warnings. % silence warnings

goal_or(0,(0;neg(0))) :- #false.

#program base.

lit(lit(F, 1)) :- fluent(F).
lit(lit(F,-1)) :- fluent(F).

complement(F,lit(F,1),lit(F,-1)) :- fluent(F).
complement(L,M;M,L) :- complement(F,L,M).

fluent((L;M),F) :- complement(F,L,M).

contradict(F)  :- complement(F,L,M), effect(A,L,N1), effect(A,M,N2),
                  #false : condition(A,M,N1), condition(A,L,N2), complement(L,M).

holds(F,0) :- holds_initially(F).

#program trans(t).

1 { occurs(A,t) : action(A) } 1.
 :- occurs(A,t), executable(A,L), not holds(F,t-1), complement(F,L,M).
 :- occurs(A,t), executable(A,M),     holds(F,t-1), complement(F,L,M).

holds(L,t) :- occurs(A,t), effect(A,L,N), holds(C,t-1) : condition(A,C,N).
holds(L,t) :- holds(L,t-1), complement(L,M), not holds(M,t).
 :- contradict(F), complement(F,L,M), holds((L;M),t).

caused(L,t) :- occurs(A,t), effect(A,L,N), 
               not holds(F',t-1) : condition(A,M',N), complement(F',L',M');
                   holds(F',t-1) : condition(A,L',N), complement(F',L',M').
holds(F,t) :- caused(L,t), complement(F,L,M).
holds(F,t) :- holds(F,t-1), complement(F,L,M), not caused(M,t).
 :- caused((L;M),t), contradict(F), complement(F,L,M).

#program state(t).
#external vol(t).

 :- goal_or(N,_);
    not holds(F,t) : goal_or(N,L), complement(F,L,M);
        holds(F,t) : goal_or(N,M), complement(F,L,M);
    vol(t).
 :- goal(L), not holds(F,t), complement(F,L,M), vol(t).
 :- goal(M),     holds(F,t), complement(F,L,M), vol(t).

#program check(t).

%%%%%%% base

alt_holds(L,0,t); alt_holds(M,0,t) :- complement(F,L,M), not initially(L;M), vol(t).
alt_holds(L,0,t) :- initially(L), vol(t).

pos_holds_oneof(N,B,t) :- vol(t), initially_oneof(N,_,B),                  alt_holds(L,0,t) : initially_oneof(N,L,B).
not_holds_oneof(N,B,t) :- vol(t), initially_oneof(N,L,B), complement(L,M), alt_holds(M,0,t).

bottom(t) :- vol(t), initially_oneof(N,_,_), 2 { pos_holds_oneof(N,B,t) : initially_oneof(N,_,B) }.
bottom(t) :- vol(t), initially_oneof(N,_,_),     not_holds_oneof(N,B,t) : initially_oneof(N,_,B).
bottom(t) :- alt_holds(L,0,t), initially_oneof(N,L,B), not_holds_oneof(N,B,t); vol(t).

%%%%%%% transition

not_condition(A,N,T,t) :- condition(A,C,N), complement(C,D), alt_holds(D,T-1,t), vol(t).
not_caused(L,T,t)      :- occurs(A,T), lit(L), not_condition(A,N,T,t) : effect(A,L,N); vol(t).

alt_holds(L,T,t) :- occurs(A,T), effect(A,L,N), alt_holds(C,T-1,t) : condition(A,C,N); vol(t).
alt_holds(L,T,t) :- alt_holds(L,T-1,t), not_caused(M,T,t), complement(L,M), vol(t).

%%%%%%% state

not_contradict(F,T,t) :- contradict(F), fluent(L,F), not_caused(L,T,t), vol(t).
pos_executable(T,t)   :- occurs(A,T), alt_holds(C,T-1,t) : executable(A,C), C != true; vol(t).
pos_goal_or(N,t) :- goal_or(N,L), alt_holds(L,t,t), vol(t).
pos_goal_or(N,t) :- goal_or(N,L), goal(L), vol(t).

bottom(t) :- pos_goal_or(N,t)      : goal_or(N,_); 
             alt_holds(L,t,t)      : goal(L);
             pos_executable(R,t)   : R = 1..t; 
             not_contradict(F,S,t) : contradict(F), S = 1..t;
             vol(t).

%%%%%%% saturation

alt_holds((L;M),0,t) :- bottom(t), complement(F,L,M), not initially(L;M), vol(t).

:- not bottom(t), vol(t).

#show occurs/2.

#program base.
