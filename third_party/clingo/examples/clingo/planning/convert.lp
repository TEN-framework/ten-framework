f(lit(F, 1),F)      :- fluent(F).
f(lit(F,-1),neg(F)) :- fluent(F).

_action(A)              :- action(A).
_fluent(A)              :- fluent(A).
_executable(A,M)        :- executable(A,L), f(M,L).
_effect(A,M,G)          :- effect(A,L,G), f(M,L).
_condition(A,M,N)       :- condition(A,L,N), f(M,L).
_initially(M)           :- initially(L), f(M,L).
_initially_oneof(G,M,B) :- initially_oneof(G,L,B), f(M,L).
_goal(M)                :- goal(L), f(M,L).
_goal_or(M,N)           :- goal_or(L,N,_), f(M,L).

complement(F,lit(F,1),lit(F,-1)) :- fluent(F).

{ _holds_initially(F) } :- complement(F,L,M), not _initially(L;M).
  _holds_initially(F)   :- _initially(L), complement(F,L,M).

holds_oneof(N,B)  :- 
    _initially_oneof(N,_,B);
    not _holds_initially(F) : _initially_oneof(N,M,B), complement(F,L,M);
        _holds_initially(F) : _initially_oneof(N,L,B), complement(F,L,M).

 :- _initially_oneof(N,_,_), not 1 { holds_oneof(N,B) } 1.
 % just bizarre!
 :- not _holds_initially(F), _initially_oneof(N,M,B), not holds_oneof(N,B), complement(F,L,M).
 :-     _holds_initially(F), _initially_oneof(N,L,B), not holds_oneof(N,B), complement(F,L,M).

