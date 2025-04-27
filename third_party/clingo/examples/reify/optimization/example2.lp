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

%%%%%%%%%%%%%%%%%%%%%%%%% subset minimization + query %%%%%%%%%%%%%%%%%%%%%%%%%

_query :- not fail.

_criteria(0,1,query) :- _query.
_criteria(0,1,occurs(A,T)) :- occurs(A,T).

_optimize(0,1,incl).

#show occurs/2.
#show _criteria/3.
#show _optimize/3.
#show _query/0.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% instance %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

fluent(cpa_at(cpa_f1, cpa_p1)).
fluent(cpa_inside(cpa_e1)).
fluent(cpa_at(cpa_f1, cpa_p0)).
fluent(cpa_have(cpa_c0)).
fluent(cpa_have(cpa_c1)).
fluent(cpa_at(cpa_f0, cpa_p1)).
fluent(cpa_inside(cpa_e0)).
fluent(cpa_at(cpa_f0, cpa_p0)).
fluent(cpa_in(cpa_e0, cpa_f0)).
fluent(cpa_in(cpa_e0, cpa_f1)).
fluent(cpa_in(cpa_e1, cpa_f0)).
fluent(cpa_in(cpa_e1, cpa_f1)).
fluent(cpa_coin_at(cpa_c0, cpa_f1, cpa_p0)).
fluent(cpa_coin_at(cpa_c0, cpa_f1, cpa_p1)).
fluent(cpa_coin_at(cpa_c1, cpa_f1, cpa_p0)).
fluent(cpa_coin_at(cpa_c1, cpa_f1, cpa_p1)).

%% actions ------ 

action(cpa_collect(cpa_c0, cpa_f0, cpa_p0)).
action(cpa_collect(cpa_c0, cpa_f0, cpa_p1)).
action(cpa_collect(cpa_c0, cpa_f1, cpa_p0)).
action(cpa_collect(cpa_c0, cpa_f1, cpa_p1)).
action(cpa_collect(cpa_c1, cpa_f0, cpa_p0)).
action(cpa_collect(cpa_c1, cpa_f0, cpa_p1)).
action(cpa_collect(cpa_c1, cpa_f1, cpa_p0)).
action(cpa_collect(cpa_c1, cpa_f1, cpa_p1)).
action(cpa_go_down(cpa_e0, cpa_f1, cpa_f0)).
action(cpa_go_down(cpa_e1, cpa_f1, cpa_f0)).
action(cpa_go_up(cpa_e0, cpa_f0, cpa_f1)).
action(cpa_go_up(cpa_e1, cpa_f0, cpa_f1)).
action(cpa_move_left(cpa_f0, cpa_p1, cpa_p0)).
action(cpa_move_left(cpa_f1, cpa_p1, cpa_p0)).
action(cpa_move_right(cpa_f0, cpa_p0, cpa_p1)).
action(cpa_move_right(cpa_f1, cpa_p0, cpa_p1)).
action(cpa_step_in(cpa_e0, cpa_f0, cpa_p0)).
action(cpa_step_in(cpa_e0, cpa_f1, cpa_p0)).
action(cpa_step_in(cpa_e1, cpa_f0, cpa_p1)).
action(cpa_step_in(cpa_e1, cpa_f1, cpa_p1)).
action(cpa_step_out(cpa_e0, cpa_f0, cpa_p0)).
action(cpa_step_out(cpa_e0, cpa_f1, cpa_p0)).
action(cpa_step_out(cpa_e1, cpa_f0, cpa_p1)).
action(cpa_step_out(cpa_e1, cpa_f1, cpa_p1)).

%% executable ------ 

executable(cpa_collect(cpa_c0, cpa_f0, cpa_p0),cpa_at(cpa_f0, cpa_p0)).
executable(cpa_collect(cpa_c0, cpa_f0, cpa_p1),cpa_at(cpa_f0, cpa_p1)).
executable(cpa_collect(cpa_c0, cpa_f1, cpa_p0),cpa_at(cpa_f1, cpa_p0)).
executable(cpa_collect(cpa_c0, cpa_f1, cpa_p1),cpa_at(cpa_f1, cpa_p1)).
executable(cpa_collect(cpa_c1, cpa_f0, cpa_p0),cpa_at(cpa_f0, cpa_p0)).
executable(cpa_collect(cpa_c1, cpa_f0, cpa_p1),cpa_at(cpa_f0, cpa_p1)).
executable(cpa_collect(cpa_c1, cpa_f1, cpa_p0),cpa_at(cpa_f1, cpa_p0)).
executable(cpa_collect(cpa_c1, cpa_f1, cpa_p1),cpa_at(cpa_f1, cpa_p1)).
executable(cpa_move_left(cpa_f0, cpa_p1, cpa_p0),cpa_at(cpa_f0, cpa_p1)).
executable(cpa_move_left(cpa_f1, cpa_p1, cpa_p0),cpa_at(cpa_f1, cpa_p1)).
executable(cpa_move_right(cpa_f0, cpa_p0, cpa_p1),cpa_at(cpa_f0, cpa_p0)).
executable(cpa_move_right(cpa_f1, cpa_p0, cpa_p1),cpa_at(cpa_f1, cpa_p0)).
executable(cpa_step_in(cpa_e0, cpa_f0, cpa_p0),cpa_at(cpa_f0, cpa_p0)).
executable(cpa_step_in(cpa_e0, cpa_f1, cpa_p0),cpa_at(cpa_f1, cpa_p0)).
executable(cpa_step_in(cpa_e1, cpa_f0, cpa_p1),cpa_at(cpa_f0, cpa_p1)).
executable(cpa_step_in(cpa_e1, cpa_f1, cpa_p1),cpa_at(cpa_f1, cpa_p1)).
executable(cpa_step_out(cpa_e0, cpa_f0, cpa_p0),cpa_inside(cpa_e0)).
executable(cpa_step_out(cpa_e0, cpa_f1, cpa_p0),cpa_inside(cpa_e0)).
executable(cpa_step_out(cpa_e1, cpa_f0, cpa_p1),cpa_inside(cpa_e1)).
executable(cpa_step_out(cpa_e1, cpa_f1, cpa_p1),cpa_inside(cpa_e1)).

%% effects ------ 

effect(cpa_collect(cpa_c0, cpa_f1, cpa_p0),cpa_have(cpa_c0),1).
effect(cpa_collect(cpa_c0, cpa_f1, cpa_p0),neg(cpa_coin_at(cpa_c0, cpa_f1, cpa_p0)),1).
condition(cpa_collect(cpa_c0, cpa_f1, cpa_p0),cpa_coin_at(cpa_c0, cpa_f1, cpa_p0),1).
effect(cpa_collect(cpa_c0, cpa_f1, cpa_p1),cpa_have(cpa_c0),1).
effect(cpa_collect(cpa_c0, cpa_f1, cpa_p1),neg(cpa_coin_at(cpa_c0, cpa_f1, cpa_p1)),1).
condition(cpa_collect(cpa_c0, cpa_f1, cpa_p1),cpa_coin_at(cpa_c0, cpa_f1, cpa_p1),1).
effect(cpa_collect(cpa_c1, cpa_f1, cpa_p0),cpa_have(cpa_c1),1).
effect(cpa_collect(cpa_c1, cpa_f1, cpa_p0),neg(cpa_coin_at(cpa_c1, cpa_f1, cpa_p0)),1).
condition(cpa_collect(cpa_c1, cpa_f1, cpa_p0),cpa_coin_at(cpa_c1, cpa_f1, cpa_p0),1).
effect(cpa_collect(cpa_c1, cpa_f1, cpa_p1),cpa_have(cpa_c1),1).
effect(cpa_collect(cpa_c1, cpa_f1, cpa_p1),neg(cpa_coin_at(cpa_c1, cpa_f1, cpa_p1)),1).
condition(cpa_collect(cpa_c1, cpa_f1, cpa_p1),cpa_coin_at(cpa_c1, cpa_f1, cpa_p1),1).
effect(cpa_go_down(cpa_e0, cpa_f1, cpa_f0),cpa_in(cpa_e0, cpa_f0),1).
effect(cpa_go_down(cpa_e0, cpa_f1, cpa_f0),neg(cpa_in(cpa_e0, cpa_f1)),1).
condition(cpa_go_down(cpa_e0, cpa_f1, cpa_f0),cpa_in(cpa_e0, cpa_f1),1).
effect(cpa_go_down(cpa_e1, cpa_f1, cpa_f0),cpa_in(cpa_e1, cpa_f0),1).
effect(cpa_go_down(cpa_e1, cpa_f1, cpa_f0),neg(cpa_in(cpa_e1, cpa_f1)),1).
condition(cpa_go_down(cpa_e1, cpa_f1, cpa_f0),cpa_in(cpa_e1, cpa_f1),1).
effect(cpa_go_up(cpa_e0, cpa_f0, cpa_f1),cpa_in(cpa_e0, cpa_f1),1).
effect(cpa_go_up(cpa_e0, cpa_f0, cpa_f1),neg(cpa_in(cpa_e0, cpa_f0)),1).
condition(cpa_go_up(cpa_e0, cpa_f0, cpa_f1),cpa_in(cpa_e0, cpa_f0),1).
effect(cpa_go_up(cpa_e1, cpa_f0, cpa_f1),cpa_in(cpa_e1, cpa_f1),1).
effect(cpa_go_up(cpa_e1, cpa_f0, cpa_f1),neg(cpa_in(cpa_e1, cpa_f0)),1).
condition(cpa_go_up(cpa_e1, cpa_f0, cpa_f1),cpa_in(cpa_e1, cpa_f0),1).
effect(cpa_move_left(cpa_f0, cpa_p1, cpa_p0),neg(cpa_at(cpa_f0, cpa_p1)),1).
effect(cpa_move_left(cpa_f0, cpa_p1, cpa_p0),cpa_at(cpa_f0, cpa_p0),1).
effect(cpa_move_left(cpa_f1, cpa_p1, cpa_p0),neg(cpa_at(cpa_f1, cpa_p1)),1).
effect(cpa_move_left(cpa_f1, cpa_p1, cpa_p0),cpa_at(cpa_f1, cpa_p0),1).
effect(cpa_move_right(cpa_f0, cpa_p0, cpa_p1),neg(cpa_at(cpa_f0, cpa_p0)),1).
effect(cpa_move_right(cpa_f0, cpa_p0, cpa_p1),cpa_at(cpa_f0, cpa_p1),1).
effect(cpa_move_right(cpa_f1, cpa_p0, cpa_p1),neg(cpa_at(cpa_f1, cpa_p0)),1).
effect(cpa_move_right(cpa_f1, cpa_p0, cpa_p1),cpa_at(cpa_f1, cpa_p1),1).
effect(cpa_step_in(cpa_e0, cpa_f0, cpa_p0),cpa_inside(cpa_e0),1).
effect(cpa_step_in(cpa_e0, cpa_f0, cpa_p0),neg(cpa_at(cpa_f0, cpa_p0)),1).
condition(cpa_step_in(cpa_e0, cpa_f0, cpa_p0),cpa_in(cpa_e0, cpa_f0),1).
effect(cpa_step_in(cpa_e0, cpa_f1, cpa_p0),cpa_inside(cpa_e0),1).
effect(cpa_step_in(cpa_e0, cpa_f1, cpa_p0),neg(cpa_at(cpa_f1, cpa_p0)),1).
condition(cpa_step_in(cpa_e0, cpa_f1, cpa_p0),cpa_in(cpa_e0, cpa_f1),1).
effect(cpa_step_in(cpa_e1, cpa_f0, cpa_p1),cpa_inside(cpa_e1),1).
effect(cpa_step_in(cpa_e1, cpa_f0, cpa_p1),neg(cpa_at(cpa_f0, cpa_p1)),1).
condition(cpa_step_in(cpa_e1, cpa_f0, cpa_p1),cpa_in(cpa_e1, cpa_f0),1).
effect(cpa_step_in(cpa_e1, cpa_f1, cpa_p1),cpa_inside(cpa_e1),1).
effect(cpa_step_in(cpa_e1, cpa_f1, cpa_p1),neg(cpa_at(cpa_f1, cpa_p1)),1).
condition(cpa_step_in(cpa_e1, cpa_f1, cpa_p1),cpa_in(cpa_e1, cpa_f1),1).
effect(cpa_step_out(cpa_e0, cpa_f0, cpa_p0),cpa_at(cpa_f0, cpa_p0),1).
effect(cpa_step_out(cpa_e0, cpa_f0, cpa_p0),neg(cpa_inside(cpa_e0)),1).
condition(cpa_step_out(cpa_e0, cpa_f0, cpa_p0),cpa_in(cpa_e0, cpa_f0),1).
effect(cpa_step_out(cpa_e0, cpa_f1, cpa_p0),cpa_at(cpa_f1, cpa_p0),1).
effect(cpa_step_out(cpa_e0, cpa_f1, cpa_p0),neg(cpa_inside(cpa_e0)),1).
condition(cpa_step_out(cpa_e0, cpa_f1, cpa_p0),cpa_in(cpa_e0, cpa_f1),1).
effect(cpa_step_out(cpa_e1, cpa_f0, cpa_p1),cpa_at(cpa_f0, cpa_p1),1).
effect(cpa_step_out(cpa_e1, cpa_f0, cpa_p1),neg(cpa_inside(cpa_e1)),1).
condition(cpa_step_out(cpa_e1, cpa_f0, cpa_p1),cpa_in(cpa_e1, cpa_f0),1).
effect(cpa_step_out(cpa_e1, cpa_f1, cpa_p1),cpa_at(cpa_f1, cpa_p1),1).
effect(cpa_step_out(cpa_e1, cpa_f1, cpa_p1),neg(cpa_inside(cpa_e1)),1).
condition(cpa_step_out(cpa_e1, cpa_f1, cpa_p1),cpa_in(cpa_e1, cpa_f1),1).

%% initial state ------ 

initially(cpa_at(cpa_f0, cpa_p0)).
initially(neg(cpa_at(cpa_f0, cpa_p1))).
initially(neg(cpa_at(cpa_f1, cpa_p0))).
initially(neg(cpa_at(cpa_f1, cpa_p1))).
initially(neg(cpa_inside(cpa_e0))).
initially(neg(cpa_inside(cpa_e1))).
initially(neg(cpa_have(cpa_c0))).
initially(neg(cpa_have(cpa_c1))).
initially_oneof(1,cpa_in(cpa_e0, cpa_f0),1).
initially_oneof(1,cpa_in(cpa_e0, cpa_f1),2).
initially_oneof(2,cpa_in(cpa_e1, cpa_f0),1).
initially_oneof(2,cpa_in(cpa_e1, cpa_f1),2).
initially_oneof(3,cpa_coin_at(cpa_c0, cpa_f1, cpa_p0),1).
initially_oneof(3,cpa_coin_at(cpa_c1, cpa_f1, cpa_p0),1).
initially_oneof(3,cpa_coin_at(cpa_c0, cpa_f1, cpa_p1),2).
initially_oneof(3,cpa_coin_at(cpa_c1, cpa_f1, cpa_p1),2).

%% goal state ---------- 

goal(cpa_have(cpa_c0)).
goal(cpa_have(cpa_c1)).

