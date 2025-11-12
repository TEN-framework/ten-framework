%%% generate guess atoms for the check part
guess(occurs(A,T)) :- occurs(A,T).

%%% The plan must not fail
:- fail.
