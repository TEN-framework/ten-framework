%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% setup %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#const horizon=9.
time(1..horizon).

complement(F,F,neg(F)) :- fluent(F).
complement(F,neg(F)) :- fluent(F).
complement(neg(F),F) :- fluent(F).

contradict(F)  :- complement(F,L,M), effect(A,L,N1), effect(A,M,N2),
                  #false : condition(A,M,N1), condition(A,L,N2), complement(L,M).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% NP generator %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

1 { occurs(A,T) : action(A) } 1 :- time(T).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% co-NP check %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% initial state

{ holds(F,0) } :- complement(F,L,M), not initially(L), not initially(M).
  holds(F,0)   :- initially(L), complement(F,L,M).

holds_oneof(N,B)  :- 
    initially_oneof(N,_,B);
    not holds(F,0) : initially_oneof(N,M,B), complement(F,L,M);
        holds(F,0) : initially_oneof(N,L,B), complement(F,L,M).

 :- initially_oneof(N,_,_), not 1 { holds_oneof(N,B) } 1.
 :- not holds(F,0), initially_oneof(N,M,B), not holds_oneof(N,B), complement(F,L,M).
 :-     holds(F,0), initially_oneof(N,L,B), not holds_oneof(N,B), complement(F,L,M).

% planning

fail :- occurs(A,T), executable(A,L), not holds(F,T-1), complement(F,L,M).
fail :- occurs(A,T), executable(A,M),     holds(F,T-1), complement(F,L,M).

holds(L,T) :- occurs(A,T), effect(A,L,N), holds(C,T-1) : condition(A,C,N).
holds(L,T) :- time(T), holds(L,T-1), complement(L,M), not holds(M,T).
fail :- contradict(F), complement(F,L,M), holds(L,T), holds(M,T).

caused(L,T) :- 
    occurs(A,T), effect(A,L,N), 
    not holds(F',T-1) : condition(A,M',N), complement(F',L',M');
        holds(F',T-1) : condition(A,L',N), complement(F',L',M').
holds(F,T) :- caused(L,T), complement(F,L,M).
holds(F,T) :- time(T), holds(F,T-1), complement(F,L,M), not caused(M,T).
fail :- caused((L),T), caused((M),T), contradict(F), complement(F,L,M).

% goal

fail :- goal(L), not holds(F,horizon), complement(F,L,M).
fail :- goal(M),     holds(F,horizon), complement(F,L,M).

#show occurs/2.
