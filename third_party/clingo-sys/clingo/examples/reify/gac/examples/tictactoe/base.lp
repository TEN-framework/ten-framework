#const n=3.

n { m(1..n,1..n) } n.

win :- I=1..n, m(I,J) : J=1..n.
win :- J=1..n, m(I,J) : I=1..n.
win :- m(I,I) : I=1..n.
win :- m(I,n+1-I) : I=1..n.

:- not win.

#show m/2.
