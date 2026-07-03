%%%%%%%%%%%%%%%%%%%%
% INPUT PREDICATES %
%%%%%%%%%%%%%%%%%%%%

vertex(1). vertex(2). vertex(3). vertex(4). vertex(5). vertex(6). vertex(7). vertex(8).
edge(1,3). edge(2,3). edge(3,4). edge(3,6). edge(4,5). edge(4,6). edge(6,7). edge(6,8).

% 1-+ 4-+-5
%   | | |
%   3-+-6-+-7
%   |     |
% 2-+     8

%%%%%%%%%%%%%%%%%%%%
% Intended Result: %
%%%%%%%%%%%%%%%%%%%%

%*
outvertex(1) outvertex(2) outvertex(3) outvertex(5) outvertex(7) outvertex(8)
outedge(1,3) outedge(2,3) outedge(3,5) outedge(3,7) outedge(3,8)
*%

%%%%%%%%%%%%%%%%%%%%%%%%%%%
% Intended Instantiation: %
%%%%%%%%%%%%%%%%%%%%%%%%%%%

%*
inner(6).
inner(4).
inner(3).
link(6,8).
link(6,7).
link(4,6).
link(4,5).
link(3,6).
link(3,4).
link(2,3).
link(1,3).
linked(1,3).
linked(2,3).
linked(3,4).
linked(3,6).
linked(4,5).
linked(4,6).
linked(6,7).
linked(6,8).
linked(1,6).
linked(1,4).
linked(2,6).
linked(2,4).
linked(3,5).
linked(3,8).
linked(3,7).
linked(4,8).
linked(4,7).
inlink(3,2).
inlink(4,3).
inlink(6,4).
oulink(3,5).
oulink(4,4).
oulink(6,2).
prefer(3,0).
prefer(4,1).
prefer(6,2).

unreduced(3,1).
unreduced(4,1).
unreduced(6,1).
linked(6,8,1).
linked(6,7,1).
linked(4,6,1).
linked(4,5,1).
linked(3,6,1).
linked(3,4,1).
linked(2,3,1).
linked(1,3,1).
irreducible(3,1).
irreducible(6,1).
unlinked(1,6,1).
unlinked(1,4,1).
unlinked(2,6,1).
unlinked(2,4,1).
unlinked(3,5,1).
unlinked(3,8,1).
unlinked(3,7,1).
unlinked(4,8,1).
unlinked(4,7,1).
reducible(4,1).
reduced(4,1).
reduced(1).

unreduced(3,2).
unreduced(6,2).
linked(3,5,2).
linked(3,6,2).
linked(6,8,2).
linked(6,7,2).
linked(2,3,2).
linked(1,3,2).
irreducible(3,2).
unlinked(4,6,2).
unlinked(3,4,2).
unlinked(1,6,2).
unlinked(2,6,2).
unlinked(3,8,2).
unlinked(3,7,2).
reducible(6,2).
reduced(6,2).
reduced(2).

unreduced(3,3).
linked(3,7,3).
linked(3,8,3).
linked(3,5,3).
linked(2,3,3).
linked(1,3,3).
irreducible(3,3).
unlinked(3,6,3).
unlinked(3,4,3).

outvertex(1).
outvertex(2).
outvertex(3).
outvertex(5).
outvertex(7).
outvertex(8).
outedge(3,7).
outedge(3,8).
outedge(3,5).
outedge(2,3).
outedge(1,3).
*%
