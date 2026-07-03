%%% translate guess atoms from guess part
occurs(A,T) :- guess(occurs(A,T)).

%%% the plan must fail
:- not fail.
