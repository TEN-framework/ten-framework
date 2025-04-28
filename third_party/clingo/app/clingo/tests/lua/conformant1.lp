#script (lua) 
function main(prg)
    local step   = 0
    local check  = false
    while true do
        parts = {}
        if step > 0 then
            parts[#parts+1] = {"step", {step}}
        else
            parts[#parts+1] = {"base", {}}
        end
        parts[#parts+1] = {"state", {step}}
        if check then
            parts[#parts+1] = {"check", {step}}
        end
        prg:release_external(clingo.Function("vol", {step-1}))
        prg:cleanup()
        prg:ground(parts)
        prg:assign_external(clingo.Function("vol", {step}), true)
        local ret = prg:solve()
        if ret.satisfiable then
            if not check and prg:get_const("nocheck") == nil then
                check = true
                parts = {}
                parts[#parts+1] = {"check", {step}}
                prg:cleanup()
                prg:ground(parts)
                ret = prg:solve()
                if ret.satisfiable then
                    break
                end
            else 
                break
            end
        end
        step = step+1
    end
end
#end.

#program warnings. % silence warnings

goal_or(0,(0;neg(0)),0) :- #false.

#program base.

lit(F)      :- fluent(F).
lit(neg(F)) :- fluent(F).

complement(F,F,neg(F)) :- fluent(F).
complement(F,neg(F)) :- fluent(F).
complement(neg(F),F) :- fluent(F).

fluent(F,F)      :- fluent(F).
fluent(neg(F),F) :- fluent(F).

contradict(F)  :- complement(F,L,M), effect(A,L,N1), effect(A,M,N2),
                  #false : condition(A,M,N1), condition(A,L,N2), complement(L,M).

{ holds(F,0) } :- complement(F,L,M), not initially(L), not initially(M).
  holds(F,0)   :- initially(L), complement(F,L,M).

holds_oneof(N,B)  :- 
    initially_oneof(N,_,B);
    not holds(F,0) : initially_oneof(N,M,B), complement(F,L,M);
        holds(F,0) : initially_oneof(N,L,B), complement(F,L,M).

 :- initially_oneof(N,_,_), not 1 { holds_oneof(N,B) } 1.
 % just bizarre!
 :- not holds(F,0), initially_oneof(N,M,B), not holds_oneof(N,B), complement(F,L,M).
 :-     holds(F,0), initially_oneof(N,L,B), not holds_oneof(N,B), complement(F,L,M).

#program step(t).

1 { occurs(A,t) : action(A) } 1.
 :- occurs(A,t), executable(A,L), not holds(F,t-1), complement(F,L,M).
 :- occurs(A,t), executable(A,M),     holds(F,t-1), complement(F,L,M).

holds(L,t) :- occurs(A,t), effect(A,L,N), holds(C,t-1) : condition(A,C,N).
holds(L,t) :- holds(L,t-1), complement(L,M), not holds(M,t).
 :- contradict(F), complement(F,L,M), holds((L),t), holds((M),t).

caused(L,t) :- occurs(A,t), effect(A,L,N), 
               not holds(F',t-1) : condition(A,M',N), complement(F',L',M');
                   holds(F',t-1) : condition(A,L',N), complement(F',L',M').
holds(F,t) :- caused(L,t), complement(F,L,M).
holds(F,t) :- holds(F,t-1), complement(F,L,M), not caused(M,t).
 :- caused((L),t), caused((M),t), contradict(F), complement(F,L,M).

#program state(t).
#external vol(t).

 :- goal_or(N,_,_);
    not holds(F,t) : goal_or(N,L,_), complement(F,L,M);
        holds(F,t) : goal_or(N,M,_), complement(F,L,M);
    vol(t).
 :- goal(L), not holds(F,t), complement(F,L,M), vol(t).
 :- goal(M),     holds(F,t), complement(F,L,M), vol(t).

#program check(t).

%%%%%%% base

alt_holds(L,0,t); alt_holds(M,0,t) :- complement(F,L,M), not initially(L), not initially(M), vol(t).
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
pos_goal_or(N,t) :- goal_or(N,L,B), alt_holds(L,t,t), vol(t).
pos_goal_or(N,t) :- goal_or(N,L,B), goal(L), vol(t).

bottom(t) :- pos_goal_or(N,t)  : goal_or(N,_,_); 
             alt_holds(L,t,t)  : goal(L);
             pos_executable(R,t)     : R = 1..t; 
             not_contradict(F,S,t) : contradict(F), S = 1..t;
             vol(t).

%%%%%%% saturation

alt_holds((L;M),0,t) :- bottom(t), complement(F,L,M), not initially(L), not initially(M), vol(t).

:- not bottom(t), vol(t).

#show occurs/2.

#program base.

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

