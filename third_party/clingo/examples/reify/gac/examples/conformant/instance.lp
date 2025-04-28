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
