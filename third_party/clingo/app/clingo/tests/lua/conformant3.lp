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

fluent(cpa_current_stage(cpa_s5)).
fluent(cpa_bad(cpa_p2)).
fluent(cpa_ok(cpa_p2)).
fluent(cpa_current_stage(cpa_s4)).
fluent(cpa_bad(cpa_p1)).
fluent(cpa_ok(cpa_p1)).
fluent(cpa_current_stage(cpa_s3)).
fluent(cpa_read(cpa_p2)).
fluent(cpa_bad(cpa_p0)).
fluent(cpa_ok(cpa_p0)).
fluent(cpa_current_stage(cpa_s2)).
fluent(cpa_read(cpa_p1)).
fluent(cpa_current_stage(cpa_s1)).
fluent(cpa_read(cpa_p0)).
fluent(cpa_current_stage(cpa_s0)).
fluent(cpa_in_channel(cpa_p0)).
fluent(cpa_in_channel(cpa_p1)).
fluent(cpa_in_channel(cpa_p2)).
fluent(cpa_noisy(cpa_p0)).
fluent(cpa_noisy(cpa_p1)).
fluent(cpa_noisy(cpa_p2)).

%% actions ------ 

action(cpa_certify(cpa_p0)).
action(cpa_certify(cpa_p1)).
action(cpa_certify(cpa_p2)).
action(cpa_reset(cpa_s1)).
action(cpa_reset(cpa_s2)).
action(cpa_reset(cpa_s3)).
action(cpa_reset(cpa_s4)).
action(cpa_reset(cpa_s5)).
action(cpa_advance(cpa_s0, cpa_s1)).
action(cpa_advance(cpa_s1, cpa_s2)).
action(cpa_advance(cpa_s2, cpa_s3)).
action(cpa_advance(cpa_s3, cpa_s4)).
action(cpa_advance(cpa_s4, cpa_s5)).
action(cpa_obtain(cpa_p0, cpa_s0)).
action(cpa_obtain(cpa_p1, cpa_s1)).
action(cpa_obtain(cpa_p2, cpa_s2)).
action(cpa_request_copy(cpa_p0, cpa_s0)).
action(cpa_request_copy(cpa_p0, cpa_s1)).
action(cpa_request_copy(cpa_p0, cpa_s2)).
action(cpa_request_copy(cpa_p0, cpa_s3)).
action(cpa_request_copy(cpa_p0, cpa_s4)).
action(cpa_request_copy(cpa_p0, cpa_s5)).
action(cpa_request_copy(cpa_p1, cpa_s0)).
action(cpa_request_copy(cpa_p1, cpa_s1)).
action(cpa_request_copy(cpa_p1, cpa_s2)).
action(cpa_request_copy(cpa_p1, cpa_s3)).
action(cpa_request_copy(cpa_p1, cpa_s4)).
action(cpa_request_copy(cpa_p1, cpa_s5)).
action(cpa_request_copy(cpa_p2, cpa_s0)).
action(cpa_request_copy(cpa_p2, cpa_s1)).
action(cpa_request_copy(cpa_p2, cpa_s2)).
action(cpa_request_copy(cpa_p2, cpa_s3)).
action(cpa_request_copy(cpa_p2, cpa_s4)).
action(cpa_request_copy(cpa_p2, cpa_s5)).

%% executable ------ 

executable(cpa_certify(cpa_p0),cpa_read(cpa_p0)).
executable(cpa_certify(cpa_p1),cpa_read(cpa_p1)).
executable(cpa_certify(cpa_p2),cpa_read(cpa_p2)).
executable(cpa_reset(cpa_s1),cpa_current_stage(cpa_s1)).
executable(cpa_reset(cpa_s2),cpa_current_stage(cpa_s2)).
executable(cpa_reset(cpa_s3),cpa_current_stage(cpa_s3)).
executable(cpa_reset(cpa_s4),cpa_current_stage(cpa_s4)).
executable(cpa_reset(cpa_s5),cpa_current_stage(cpa_s5)).
executable(cpa_advance(cpa_s0, cpa_s1),cpa_current_stage(cpa_s0)).
executable(cpa_advance(cpa_s1, cpa_s2),cpa_current_stage(cpa_s1)).
executable(cpa_advance(cpa_s2, cpa_s3),cpa_current_stage(cpa_s2)).
executable(cpa_advance(cpa_s3, cpa_s4),cpa_current_stage(cpa_s3)).
executable(cpa_advance(cpa_s4, cpa_s5),cpa_current_stage(cpa_s4)).
executable(cpa_obtain(cpa_p0, cpa_s0),cpa_current_stage(cpa_s0)).
executable(cpa_obtain(cpa_p1, cpa_s1),cpa_current_stage(cpa_s1)).
executable(cpa_obtain(cpa_p2, cpa_s2),cpa_current_stage(cpa_s2)).
executable(cpa_request_copy(cpa_p0, cpa_s0),cpa_current_stage(cpa_s0)).
executable(cpa_request_copy(cpa_p0, cpa_s1),cpa_current_stage(cpa_s1)).
executable(cpa_request_copy(cpa_p0, cpa_s2),cpa_current_stage(cpa_s2)).
executable(cpa_request_copy(cpa_p0, cpa_s3),cpa_current_stage(cpa_s3)).
executable(cpa_request_copy(cpa_p0, cpa_s4),cpa_current_stage(cpa_s4)).
executable(cpa_request_copy(cpa_p0, cpa_s5),cpa_current_stage(cpa_s5)).
executable(cpa_request_copy(cpa_p1, cpa_s0),cpa_current_stage(cpa_s0)).
executable(cpa_request_copy(cpa_p1, cpa_s1),cpa_current_stage(cpa_s1)).
executable(cpa_request_copy(cpa_p1, cpa_s2),cpa_current_stage(cpa_s2)).
executable(cpa_request_copy(cpa_p1, cpa_s3),cpa_current_stage(cpa_s3)).
executable(cpa_request_copy(cpa_p1, cpa_s4),cpa_current_stage(cpa_s4)).
executable(cpa_request_copy(cpa_p1, cpa_s5),cpa_current_stage(cpa_s5)).
executable(cpa_request_copy(cpa_p2, cpa_s0),cpa_current_stage(cpa_s0)).
executable(cpa_request_copy(cpa_p2, cpa_s1),cpa_current_stage(cpa_s1)).
executable(cpa_request_copy(cpa_p2, cpa_s2),cpa_current_stage(cpa_s2)).
executable(cpa_request_copy(cpa_p2, cpa_s3),cpa_current_stage(cpa_s3)).
executable(cpa_request_copy(cpa_p2, cpa_s4),cpa_current_stage(cpa_s4)).
executable(cpa_request_copy(cpa_p2, cpa_s5),cpa_current_stage(cpa_s5)).

%% effects ------ 

effect(cpa_certify(cpa_p0),cpa_bad(cpa_p0),1).
condition(cpa_certify(cpa_p0),cpa_noisy(cpa_p0),1).
effect(cpa_certify(cpa_p0),cpa_ok(cpa_p0),2).
condition(cpa_certify(cpa_p0),neg(cpa_noisy(cpa_p0)),2).
effect(cpa_certify(cpa_p1),cpa_bad(cpa_p1),1).
condition(cpa_certify(cpa_p1),cpa_noisy(cpa_p1),1).
effect(cpa_certify(cpa_p1),cpa_ok(cpa_p1),2).
condition(cpa_certify(cpa_p1),neg(cpa_noisy(cpa_p1)),2).
effect(cpa_certify(cpa_p2),cpa_bad(cpa_p2),1).
condition(cpa_certify(cpa_p2),cpa_noisy(cpa_p2),1).
effect(cpa_certify(cpa_p2),cpa_ok(cpa_p2),2).
condition(cpa_certify(cpa_p2),neg(cpa_noisy(cpa_p2)),2).
effect(cpa_reset(cpa_s1),neg(cpa_current_stage(cpa_s1)),1).
effect(cpa_reset(cpa_s1),cpa_current_stage(cpa_s0),1).
effect(cpa_reset(cpa_s2),neg(cpa_current_stage(cpa_s2)),1).
effect(cpa_reset(cpa_s2),cpa_current_stage(cpa_s0),1).
effect(cpa_reset(cpa_s3),neg(cpa_current_stage(cpa_s3)),1).
effect(cpa_reset(cpa_s3),cpa_current_stage(cpa_s0),1).
effect(cpa_reset(cpa_s4),neg(cpa_current_stage(cpa_s4)),1).
effect(cpa_reset(cpa_s4),cpa_current_stage(cpa_s0),1).
effect(cpa_reset(cpa_s5),neg(cpa_current_stage(cpa_s5)),1).
effect(cpa_reset(cpa_s5),cpa_current_stage(cpa_s0),1).
effect(cpa_advance(cpa_s0, cpa_s1),cpa_current_stage(cpa_s1),1).
effect(cpa_advance(cpa_s0, cpa_s1),neg(cpa_current_stage(cpa_s0)),1).
effect(cpa_advance(cpa_s1, cpa_s2),cpa_current_stage(cpa_s2),1).
effect(cpa_advance(cpa_s1, cpa_s2),neg(cpa_current_stage(cpa_s1)),1).
effect(cpa_advance(cpa_s2, cpa_s3),cpa_current_stage(cpa_s3),1).
effect(cpa_advance(cpa_s2, cpa_s3),neg(cpa_current_stage(cpa_s2)),1).
effect(cpa_advance(cpa_s3, cpa_s4),cpa_current_stage(cpa_s4),1).
effect(cpa_advance(cpa_s3, cpa_s4),neg(cpa_current_stage(cpa_s3)),1).
effect(cpa_advance(cpa_s4, cpa_s5),cpa_current_stage(cpa_s5),1).
effect(cpa_advance(cpa_s4, cpa_s5),neg(cpa_current_stage(cpa_s4)),1).
effect(cpa_obtain(cpa_p0, cpa_s0),neg(cpa_in_channel(cpa_p0)),1).
effect(cpa_obtain(cpa_p0, cpa_s0),cpa_read(cpa_p0),1).
condition(cpa_obtain(cpa_p0, cpa_s0),cpa_in_channel(cpa_p0),1).
effect(cpa_obtain(cpa_p1, cpa_s1),neg(cpa_in_channel(cpa_p1)),1).
effect(cpa_obtain(cpa_p1, cpa_s1),cpa_read(cpa_p1),1).
condition(cpa_obtain(cpa_p1, cpa_s1),cpa_in_channel(cpa_p1),1).
effect(cpa_obtain(cpa_p2, cpa_s2),neg(cpa_in_channel(cpa_p2)),1).
effect(cpa_obtain(cpa_p2, cpa_s2),cpa_read(cpa_p2),1).
condition(cpa_obtain(cpa_p2, cpa_s2),cpa_in_channel(cpa_p2),1).
effect(cpa_request_copy(cpa_p0, cpa_s0),cpa_in_channel(cpa_p0),1).
effect(cpa_request_copy(cpa_p0, cpa_s0),neg(cpa_read(cpa_p0)),1).
effect(cpa_request_copy(cpa_p0, cpa_s0),neg(cpa_noisy(cpa_p0)),1).
effect(cpa_request_copy(cpa_p0, cpa_s0),neg(cpa_bad(cpa_p0)),1).
condition(cpa_request_copy(cpa_p0, cpa_s0),cpa_bad(cpa_p0),1).
effect(cpa_request_copy(cpa_p1, cpa_s1),cpa_in_channel(cpa_p1),1).
effect(cpa_request_copy(cpa_p1, cpa_s1),neg(cpa_read(cpa_p1)),1).
effect(cpa_request_copy(cpa_p1, cpa_s1),neg(cpa_noisy(cpa_p1)),1).
effect(cpa_request_copy(cpa_p1, cpa_s1),neg(cpa_bad(cpa_p1)),1).
condition(cpa_request_copy(cpa_p1, cpa_s1),cpa_bad(cpa_p1),1).
effect(cpa_request_copy(cpa_p2, cpa_s2),cpa_in_channel(cpa_p2),1).
effect(cpa_request_copy(cpa_p2, cpa_s2),neg(cpa_read(cpa_p2)),1).
effect(cpa_request_copy(cpa_p2, cpa_s2),neg(cpa_noisy(cpa_p2)),1).
effect(cpa_request_copy(cpa_p2, cpa_s2),neg(cpa_bad(cpa_p2)),1).
condition(cpa_request_copy(cpa_p2, cpa_s2),cpa_bad(cpa_p2),1).

%% initial state ------ 

initially(cpa_current_stage(cpa_s0)).
initially(cpa_in_channel(cpa_p0)).
initially(cpa_in_channel(cpa_p1)).
initially(cpa_in_channel(cpa_p2)).
initially(neg(cpa_current_stage(cpa_s1))).
initially(neg(cpa_current_stage(cpa_s2))).
initially(neg(cpa_current_stage(cpa_s3))).
initially(neg(cpa_current_stage(cpa_s4))).
initially(neg(cpa_current_stage(cpa_s5))).
initially(neg(cpa_read(cpa_p0))).
initially(neg(cpa_read(cpa_p1))).
initially(neg(cpa_read(cpa_p2))).
initially(neg(cpa_bad(cpa_p0))).
initially(neg(cpa_bad(cpa_p1))).
initially(neg(cpa_bad(cpa_p2))).
initially(neg(cpa_ok(cpa_p0))).
initially(neg(cpa_ok(cpa_p1))).
initially(neg(cpa_ok(cpa_p2))).
initially_oneof(1,cpa_noisy(cpa_p0),1).
initially_oneof(1,cpa_noisy(cpa_p2),1).
initially_oneof(1,cpa_noisy(cpa_p1),1).
initially_oneof(1,neg(cpa_noisy(cpa_p0)),2).
initially_oneof(1,neg(cpa_noisy(cpa_p2)),2).
initially_oneof(1,neg(cpa_noisy(cpa_p1)),2).

%% goal state ---------- 

goal(cpa_ok(cpa_p0)).
goal(cpa_ok(cpa_p1)).
goal(cpa_ok(cpa_p2)).

