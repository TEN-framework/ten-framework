#script (lua) 
function main(prg)
    prg:ground({{"base", {}}, {"plan_graph_base", {}}})

    for step = 1,3,1 do
        prg:ground({{"plan_graph_step", {step}}})
        prg:solve()
    end
--  Comment line 8 and uncomment line 11
--  prg:solve()
end
#end.

#program plan_graph_base.
valid_f(FLUENT, 1) :- init(FLUENT).
valid_f1(FLUENT, 1) :- init(FLUENT).

#program plan_graph_step(time).

%Encoding 1

valid_a(ACT, time) :- action(ACT); valid_f(FLUENT, time) : pre(ACT, FLUENT);
            not mutex(F1, F2, time) : req_both(ACT, F1, F2).

valid_pre(ACT, FLUENT, time) :- valid_a(ACT, time), pre(ACT, FLUENT).
valid_add(ACT, FLUENT, time) :- valid_a(ACT, time), addadd(ACT, FLUENT).
valid_f(FLUENT, time + 1) :- valid_add(_, FLUENT, time).

mutex_actions(A1, A2, time) :- valid_a(A1, time), valid_a(A2, time), conflicting(A1, A2).

mutex_a_with_f(A1, P2, time) :- mutex(P1, P2, time), valid_pre(A1, P1, time).
mutex_actions(A1, A2, time) :- mutex_a_with_f(A1, P2, time), valid_pre(A2, P2, time).
%mutex_actions(A1, A2, time) :- mutex(P1, P2, time), valid_pre(A1, P1, time), valid_pre(A2, P2, time).

mutex_a_sym(A1, A2, time; A2, A1, time) :- mutex_actions(A1, A2, time).

closes(A2, F1, time) :- valid_a(A2, time), valid_f(F1, time+1); mutex_a_sym(A1, A2, time) : valid_add(A1, F1, time).
mutex(F1, F2, time+1) :- F1 < F2, valid_f(F1, time+1), valid_f(F2, time+1);
            closes(A2, F1, time) : valid_add(A2, F2, time).

% Encoding 2:
% Identical to encoding 1 except for lines 32 and 33 vs. line 57.
% However these should generate identical groundings, but they don't for some reason

valid_a1(ACT, time) :- action(ACT); valid_f1(FLUENT, time) : pre(ACT, FLUENT);
            not mutex1(F1, F2, time) : req_both(ACT, F1, F2).

valid_pre1(ACT, FLUENT, time) :- valid_a1(ACT, time), pre(ACT, FLUENT).
valid_add1(ACT, FLUENT, time) :- valid_a1(ACT, time), addadd(ACT, FLUENT).
valid_f1(FLUENT, time + 1) :- valid_add1(_, FLUENT, time).

mutex_actions1(A1, A2, time) :- valid_a1(A1, time), valid_a1(A2, time), conflicting(A1, A2).

%mutex_a_with_f1(A1, P2, time) :- mutex1(P1, P2, time), valid_pre1(A1, P1, time).
%mutex_actions1(A1, A2, time) :- mutex_a_with_f1(A1, P2, time), valid_pre1(A2, P2, time).
mutex_actions1(A1, A2, time) :- mutex1(P1, P2, time), valid_pre1(A1, P1, time), valid_pre1(A2, P2, time).

mutex_a_sym1(A1, A2, time; A2, A1, time) :- mutex_actions1(A1, A2, time).

closes1(A2, F1, time) :- valid_a1(A2, time), valid_f1(F1, time+1); mutex_a_sym1(A1, A2, time) : valid_add1(A1, F1, time).
mutex1(F1, F2, time+1) :- F1 < F2, valid_f1(F1, time+1), valid_f1(F2, time+1);
            closes1(A2, F1, time) : valid_add1(A2, F2, time).

% diff reports if the two encodings are not the same

diff(A1, A2, time) :- mutex_actions1(A1, A2, time), not mutex_actions(A1, A2, time).
diff(A1, A2, time) :- mutex_actions(A1, A2, time), not mutex_actions1(A1, A2, time).

#program base.

#show diff/3.

preserve_action(preserve(F)) :- fact(F).
action(A) :- preserve_action(A).
pre(preserve(F), F) :- fact(F).
addadd(preserve(F), F) :- fact(F).

prepre(A, F) :- pre(A, F), not del(A, F).
deldel(A, F) :- del(A, F), not pre(A, F).
predel(A, F) :- pre(A, F), del(A, F).
addadd(A, F) :- add(A, F), not pre(A, F), not del(A, F).

conflicting(A1, A2) :- pre(A1, F), del(A2, F), A1 != A2.

req_both(ACT, F1, F2) :- pre(ACT, F1), pre(ACT, F2), F1 < F2.

type(object).
type(ferry, object).
fact(at_ferry(X)) :- type(X, object).
fact(at(X, Y)) :- type(X, object), type(Y, object).
fact(on(X, Y)) :- type(X, object), type(Y, object).
fact(empty_ferry).
action(debark(X, Y)) :- type(X, object), type(Y, object), auto__(X), place__(Y).
pre(debark(X, Y), on(X, ferry)) :- action(debark(X, Y)).
pre(debark(X, Y), at_ferry(Y)) :- action(debark(X, Y)).
add(debark(X, Y), at(X, Y)) :- action(debark(X, Y)).
add(debark(X, Y), empty_ferry) :- action(debark(X, Y)).
del(debark(X, Y), on(X, ferry)) :- action(debark(X, Y)).
action(sail(X, Y)) :- type(X, object), type(Y, object), place__(X), place__(Y).
pre(sail(X, Y), at_ferry(X)) :- action(sail(X, Y)).
add(sail(X, Y), at_ferry(Y)) :- action(sail(X, Y)).
del(sail(X, Y), at_ferry(X)) :- action(sail(X, Y)).
action(board(X, Y)) :- type(X, object), type(Y, object), place__(Y), auto__(X).
pre(board(X, Y), at(X, Y)) :- action(board(X, Y)).
pre(board(X, Y), at_ferry(Y)) :- action(board(X, Y)).
pre(board(X, Y), empty_ferry) :- action(board(X, Y)).
add(board(X, Y), on(X, ferry)) :- action(board(X, Y)).
del(board(X, Y), at(X, Y)) :- action(board(X, Y)).
del(board(X, Y), empty_ferry) :- action(board(X, Y)).
type(c1, object).
type(b, object).
type(c2, object).
type(a, object).
place__(a).
place__(b).
auto__(c1).
auto__(c2).
init(at(c1, a)).
init(at(c2, a)).
init(at_ferry(a)).
init(empty_ferry).
goal(at(c1, b)).
goal(at(c2, b)).

