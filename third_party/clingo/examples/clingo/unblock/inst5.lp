brick(1,1,2). %%
brick(1,2,2).

brick(2,3,2). %%
brick(2,4,2).

brick(3,6,4). %
brick(3,6,3). %
brick(3,6,2). %

brick(4,1,3). %%
brick(4,2,3).

brick(5,3,3). %%
brick(5,4,3).

brick(6,1,6). %
brick(6,1,5). %
brick(6,1,4). %

brick(r,2,4). %%
brick(r,3,4).

brick(7,4,4). %
brick(7,4,5). %

brick(8,3,5). %
brick(8,3,6). %

brick(9,6,6). %%%
brick(9,5,6).
brick(9,4,6).

brick(I) :- brick(I,_,_).
count(I,L) :- brick(I), L = { brick(I,X,Y) }.
stone(I,x,X,Y,L) :- not brick(I,X-1,Y), brick(I,X,Y), brick(I,X+1,Y), count(I,L).
stone(I,y,X,Y,L) :- not brick(I,X,Y-1), brick(I,X,Y), brick(I,X,Y+1), count(I,L).

target(r,5,4).

field(1..6,1..6).

