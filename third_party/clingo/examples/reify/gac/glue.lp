#include "../common/metaD.lp".

bot :- output(guess(X),B),     guess(X), fail(normal(B)).
bot :- output(guess(X),B), not guess(X), true(normal(B)).

:- not bot.
