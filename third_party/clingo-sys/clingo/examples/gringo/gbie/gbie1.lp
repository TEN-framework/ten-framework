char_to_digit(0, 0).
char_to_digit(1, 1).
char_to_digit(2, 2).
char_to_digit(3, 3).
char_to_digit(4, 4).
char_to_digit(5, 5).
char_to_digit(6, 6).
char_to_digit(7, 7).
char_to_digit(8, 8).
char_to_digit(9, 9).

digit(X, Y, D) :- char(C, Y), char_to_digit(C, D), X = Y - 1, X >= 0.
dig(X) :- digit(A, X, B).

sign(X, 1) :- char(p, X).
sign(X, Y) :- char(m, X), Y = 0 - 1.

num_1(X, Y, Val) :- digit(X, Y, Val), not dig(X).
num_1(X, Z, NewVal) :- num_1(X, Y, Val_1), digit(Y, Z, Val_2), NewVal_1 = 10 * Val_1, NewVal = NewVal_1 + Val_2.
num(X, Y, Val) :- num_1(X, Y, Val), Z = Y + 1, not dig(Z).

par_expr(X, W, Val) :- char(o, Y), expr(Y, Z, Val), W = Z + 1, char(c, W), X = Y - 1, X >= 0.

sing_term(X, Z, Val) :- par_expr(Y, Z, Val1), sign(Y, S), Val = Val1 * S, X = Y - 1, X >= 0.
sing_term(X, Z, Val) :- num(Y, Z, Val1), sign(Y, S), Val = Val1 * S, X = Y - 1, X >= 0.

expr(X, Y, Val) :- char(o, X), num(X, Y, Val).
expr(0, Y, Val) :- num(0, Y, Val).
expr(X, Y, Val) :- par_expr(X, Y, Val).
expr(X, Y, Val) :- sing_term(X, Y, Val).

expr(X, Z, Val) :- expr(X, Y, Val_1), sing_term(Y, Z, Val_2), Val = Val_1 + Val_2.

cmp(X, W, Val) :- char(g, Y), num(Y, W, Val), X = Y - 1, X >= 0.
cmp(X, W, Val) :- char(g, Y), Z = Y + 1, sign(Z, S), num(Z, W, Val1), Val = S * Val1, X = Y - 1, X >= 0.

sol :- expr(0, Y, Val1), cmp(Y, Z, Val2), Val1 > Val2.

val1(Y, p, V) :- expr(0, Y, V), V >= 0.
val1(Y, m, V) :- expr(0, Y, V1), V1 < 0, V = 0 - V1.
val2(Y, p, V) :- val1(Y, A, B), cmp(Y, Z, V), V >= 0.
val2(Y, m, V) :- val1(Y, A, B), cmp(Y, Z, V1), V1 < 0, V = 0 - V1.

values(S1, V1, S2, V2) :- val1(Y, S1, V1), val2(Y, S2, V2).
fail :- not sol, not fail.

#show values/4.
