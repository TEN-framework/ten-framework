char_to_digit(X,X) :- X=0..9.

digit(Y-1,Y,D) :- char(C,Y), char_to_digit(C,D), Y > 0.

sign(X, 1) :- char(p,X).
sign(X,-1) :- char(m,X).

num_1(X,Y,A)      :- digit(X,Y,A), not digit(_,X,_).
num_1(X,Z,10*A+B) :- num_1(X,Y,A), digit(Y,Z,B).
num(X,Y,A)        :- num_1(X,Y,A), not digit(_,Y+1,_).

par_expr(Y-1,Z+1,A) :- char(o,Y), expr(Y,Z,A), char(c,Z+1), Y >= 0.

sign_expr(Y-1,Z,A*S) :- par_expr(Y,Z,A), sign(Y,S), Y > 0.
sign_expr(Y-1,Z,A*S) :- num(Y,Z,A),      sign(Y,S), Y > 0.

expr(0,Y,A)   :- num(0,Y,A).
expr(X,Y,A)   :- char(o,X), num(X,Y,A).
expr(X,Y,A)   :- par_expr(X,Y,A).
expr(X,Y,A)   :- sign_expr(X,Y,A).
expr(X,Z,A+B) :- expr(X,Y,A), sign_expr(Y,Z,B).

cmp(Y-1,A)   :- char(g,Y), num(Y,_,A), Y > 0.
cmp(Y-1,A*S) :- char(g,Y), sign(Y+1,S), num(Y+1,_,A), Y > 0.

eq(A,A) :- expr(0,Y,A), cmp(Y,A).
lt(A,B) :- expr(0,Y,A), cmp(Y,B), A < B.
gt(A,B) :- expr(0,Y,A), cmp(Y,B), A > B.

#show eq/2.
#show lt/2.
#show gt/2.
