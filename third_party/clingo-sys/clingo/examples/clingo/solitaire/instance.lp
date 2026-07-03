%     -------
%     |o|o|o|
%     -------
%     |o|o|o|
% ---------------
% |o|o|o|o|o|o|o|
% ---------------
% |o|o|o| |o|o|o|
% ---------------
% |o|o|o|o|o|o|o|
% ---------------
%     |o|o|o|
%     -------
%     |o|o|o|
%     -------
%
%

field(0,0).
field(X,Y) :- stone(X,Y).

stone(-1.. 1,-3..-1).
stone(-3..-1,-1.. 1).
stone( 1.. 3,-1.. 1).
stone(-1.. 1, 1.. 3).

dir(west;east;north;south).

dir(west ,-1, 0).
dir(east , 1, 0).
dir(north, 0,-1).
dir(south, 0, 1).

goal(0,0).

% fix the first jump to avoid symmetries
fix_jump(0,-2,south,1).
