#include "../common/meta.lp".
#include "../common/metaD.lp".

cxopt(card).  cxopt(incl).  cxopt(pref).

criteria(J,W,Q,A) :- output(_criteria(J,W,Q),A).
criteria(J,W,A) :- criteria(J,W,Q,A).

cxopt(J,W,O)    :- criteria(J,W,_,_), cxopt(O), optimize(J,W,O).
cxopt(J,W,card) :- criteria(J,W,_,_), #false : optimize(J,W,O).

%%%%%%%%%% verify dominance

equal(J) :- cxopt(J,_,_), equal(J,W,O) : cxopt(J,W,O).

chain(J1,J2) :- cxopt(J1,_,_), cxopt(J2,_,_), J2 < J1,
                #false : cxopt(J3,W,O), J2 < J3, J3 < J1.

check(J2) :- cxopt(J2,_,_), #false : chain(J1,J2).
check(J2) :- chain(J1,J2), check(J1), equal(J1).

bot :- #false : cxopt(J,W,O).
bot :- check(J1), worse(J1).
bot :- check(J1), equal(J1), #false : chain(J1,J2).

% require non-existence of dominating answer set

:- not bot.

%%%%%%%%%% check cardinality criteria

equal(J,W,card) :- cxopt(J,W,card), #sum { -1,Q : not not conjunction(A),  criteria(J,W,Q,A) ;
                                            1,Q :         true(normal(A)), criteria(J,W,Q,A) } >= 0.
worse(J)        :- cxopt(J,W,card), #sum { -1,Q : not not conjunction(A),  criteria(J,W,Q,A) ;
                                            1,Q :         true(normal(A)), criteria(J,W,Q,A) } >= 1.
%%%%%%%%%% check inclusion criteria

ndiff(A) :- cxopt(J,W,incl), criteria(J,W,A), true(normal(A)).
ndiff(A) :- cxopt(J,W,incl), criteria(J,W,A), not conjunction(A).

equal(J,W,incl) :- cxopt(J,W,incl), ndiff(A) : criteria(J,W,A).
worse(J)        :- cxopt(J,W,incl), criteria(J,W,A), true(normal(A)), not conjunction(A).

%%%%%%%%%% check preference criteria (relative to user predicate prefer/2)

cando(A) :- cxopt(J,W,pref), criteria(J,W,A), fail(normal(A)),     conjunction(A).
nocan(A) :- cxopt(J,W,pref), criteria(J,W,A), true(normal(A)).
nocan(A) :- cxopt(J,W,pref), criteria(J,W,A), not conjunction(A).

condo(A) :- cxopt(J,W,pref), criteria(J,W,A), true(normal(A)), not conjunction(A).
nocon(A) :- cxopt(J,W,pref), criteria(J,W,A), fail(normal(A)).
nocon(A) :- cxopt(J,W,pref), criteria(J,W,A),     conjunction(A).

cando(J,W,A)    :- cxopt(J,W,pref), criteria(J,W,A), criteria(J,W,A'), prefer(A,A'), A != A', cando(A), condo(A').

nocon(J,W,A)    :- cxopt(J,W,pref), criteria(J,W,A), nocon(A).
nocon(J,W,A)    :- cxopt(J,W,pref), criteria(J,W,A), nocan(A') : criteria(J,W,A'), A != A', prefer(A,A').
nocon(J,W,A)    :- cxopt(J,W,pref), criteria(J,W,A), criteria(J,W,A'), prefer(A',A), not prefer(A,A'), cando(A').

equal(J,W,pref) :- cxopt(J,W,pref), criteria(J,W,A), cando(J,W,A), nocon(A') : criteria(J,W,A'), prefer(A',A), not prefer(A,A').

worse(J)        :- cxopt(J,W,pref), nocon(J,W,A) : criteria(J,W,A).

%%%%%%%%%% handling fo queries and optimization statements

optimize(J,W,Q) :- output(_optimize(J,W,Q),B), conjunction(B).
 :- output(_query,B), not conjunction(B).

hide(_criteria(J,W,Q)) :- output(_criteria(J,W,Q),_).
hide(_query)           :- output(_query,_).
hide(_optimize(J,W,Q)) :- output(_optimize(J,W,Q),_).
