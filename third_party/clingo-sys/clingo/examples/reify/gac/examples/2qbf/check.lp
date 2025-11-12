% guess values for the universally quantified variables y(1..m)
#const m=2.
{ y(1..m) }.

% false if f(x1, ..., xn, y1, ..., yn) is false
% * every body corresponds to the negation of a clause (it is a nogood)
% * in this case f is this set of clauses:
%     -x(1) v -y(1)
%     -x(2) v -y(2)
%
false :- guess(x(1)), y(1).
false :- guess(x(2)), y(2).

% inforce false
:- not false.
