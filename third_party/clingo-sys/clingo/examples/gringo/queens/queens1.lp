#const n = 10.
n(1..n).

q(X,Y) :- n(X), n(Y), not not q(X,Y).

        c(r,X; c,Y) :- q(X,Y).
not not c(r,N; c,N) :- n(N).

n(r,X,Y-1,X,Y; c,X-1,Y,X,Y; d1,X-1,Y-1,X,Y;     d2,X-1,Y+1,X,Y      ) :- n(X), n(Y).
c(r,N,0;       c,0,N;       d1,N-1,0; d1,0,N-1; d2,N-1,n+1; d2,0,N+1) :- n(N).

c(C,XX,YY) :-     c(C,X,Y), n(C,X,Y,XX,YY), not q(XX,YY).
           :- not c(C,X,Y), n(C,X,Y,XX,YY),     q(XX,YY).

#show q/2.
