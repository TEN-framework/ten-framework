%
% EXISTS x(1..n) FORALL y(1..m) f(x(1),...,x(n),y(1),...,y(m))
%

% guess values for the existentially quantified variables x(1..n)
#const n=2.
{ x(1..n) }.

% display
#show x/1.
