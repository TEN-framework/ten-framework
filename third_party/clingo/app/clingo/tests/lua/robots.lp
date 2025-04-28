#script (lua)

function main(prg)
    step = 1
    parts = {}
    while true do
        parts = {}
        if step > 1 then
            prg:release_external(clingo.Function("volatile", {step-1}))
        else
            parts[#parts+1] = {"base", {}}
        end
        parts[#parts+1] = {"cumulative", {step}}
        prg:ground(parts)
        prg:assign_external(clingo.Function("volatile", {step}), true)
        ret = prg:solve()
        if ret.satisfiable then
            break
        end
        step = step + 1
    end
end

#end.

#program base.

dim(1..dimension).

robot(red).
robot(blue).
robot(green).
robot(yellow).

direction(north).
direction(east).
direction(south).
direction(west).

% blockages
blocked(I,J,D) :- barrier(I,J,D),direction(D),dim(I),dim(J).

blocked(1..dimension,1           ,north).
blocked(   dimension,1..dimension,east ).
blocked(1..dimension,   dimension,south).
blocked(           1,1..dimension,west ).

blocked(I+1,J,west ) :- blocked(I,J,east ), dim(I), dim(J), I<dimension.
blocked(I,J+1,north) :- blocked(I,J,south), dim(I), dim(J), J<dimension.
blocked(I-1,J,east ) :- blocked(I,J,west ), dim(I), dim(J), I>1.
blocked(I,J-1,south) :- blocked(I,J,north), dim(I), dim(J), J>1.

% this should simplify sth.
dir(west, -1,  0).
dir(east,  1,  0).
dir(north, 0, -1).
dir(south, 0,  1).

#program cumulative(t).

% which places are reachable by a robot
reachable(R, I + DI, J + DJ, D, t) :- not blocked2(I, J, t), dir(D, DI, DJ), go(R, D, t), position(R, I, J, t - 1), dim(I), dim(J).
reachable(R, I + DI, J + DJ, D, t) :- not blocked2(I, J, t), reachable(R, I, J, D, t), dir(D, DI, DJ), go(R, D, t), dim(I), dim(J).

% no crossing of blockages or robots
blocked2(I, J, t) :- blocked(I, J, D), go(R, D, t).
blocked2(I - DI, J - DJ, t) :- position(R, I, J, t - 1), selectDir(D, t), dir(D, DI, DJ).

% move it (no stopping if there is neither a blockage nor a robot)
position(R, I, J, t) :- reachable(R, I, J, D, t), not reachable(R, I + DI, J + DJ, D, t), dir(D, DI, DJ), go(R, D, t).

% one has to be moved
moved(t) :- reachable(R, I, J, D, t).
 :- not moved(t).

% inertia
position(R, I, J, t) :- position(R, I, J, t - 1), not selectRobot(R, t), robot(R), dim(I),dim(J).

% plan generator
1 { selectRobot(R, t) : robot(R) } 1.
1 { selectDir(D,t)    : direction(D) } 1.

go(R, D, t) :- selectRobot(R, t), selectDir(D,t).

%#show moved/4.
#show go/3.
#show position/4.
%#show reachable/4.

% Authentic board from Ricochet Robots (Rasende Roboter) by Alex Randolph

#const dimension=16.

#external volatile(t).

% --- Uncomment an integrity constraint to pose a query
%   - numbers indicated minimum number of moves  
%   - sometimes a heuristics was better than another

:- instance(1), not position(red,      5,2,t), volatile(t).  % red moon:        9, vmtf 
:- instance(2), not position(red,     15,2,t), volatile(t).  % red triangle:   10, ?
:- instance(3), not position(green,    2,3,t), volatile(t).  % green triangle:  9, berkmin
:- instance(4), not position(blue,    12,3,t), volatile(t).  % blue star:       8, ?
:- instance(5), not position(yellow,   7,4,t), volatile(t).  % yellow star:    11, ?
:- instance(6), not position(blue,     4,7,t), volatile(t).  % blue saturn:     8, ?
:- instance(7), not position(green,   14,7,t), volatile(t).  % green moon:      3, ?
:- instance(8), not position(yellow,  11,8,t), volatile(t).  % yellow saturn:  13, vmtf??
:- instance(9), not position(yellow,  5,10,t), volatile(t).  % yellow moon:     2, ?
:- instance(10), not position(green,  2,11,t), volatile(t).  % green star:     11, ?
:- instance(11), not position(red,   14,11,t), volatile(t).  % red star:        9, ?
:- instance(12), not position(green, 11,12,t), volatile(t).  % green saturn:   11, ?
:- instance(13), not position(yellow,15,13,t), volatile(t).  % yellow star:     9, ?
:- instance(14), not position(blue,   7,14,t), volatile(t).  % blue star:       6, ?
:- instance(15), not position(red,    3,15,t), volatile(t).  % red saturn:      5, ?
:- instance(16), not position(blue,  10,15,t), volatile(t).  % blue moon:      12, vsids?

%:- not position(red,1,1,t), t==0.
%:- not position(blue,1,16,t), t==0.
%:- not position(green,16,1,t), t==0.
%:- not position(yellow,16,16,t), t==0.
%:- not go(red,east,t), t==1.
:- not position(blue,1,16,t), t==1.
%:- not position(green,16,1,t), t==1.
%:- not position(yellow,16,16,t), t==1.
%:- not position(red,2,1,t), t==1.
%:- not go(red,south,t), t==2.
%:- not position(blue,1,16,t), t==2.
:- not position(green,16,1,t), t==2.
%:- not position(yellow,16,16,t), t==2.
%:- not position(red,2,2,t), t==2.
%:- not go(red,east,t), t==3.
%:- not position(blue,1,16,t), t==3.
%:- not position(green,16,1,t), t==3.
%:- not position(yellow,16,16,t), t==3.
:- not position(red,4,2,t), t==3.
%:- not go(red,north,t), t==4.
%:- not position(blue,1,16,t), t==4.
%:- not position(green,16,1,t), t==4.
%:- not position(yellow,16,16,t), t==4.
%:- not position(red,4,1,t), t==4.
%:- not go(red,west,t), t==5.
%:- not position(blue,1,16,t), t==5.
%:- not position(green,16,1,t), t==5.
%:- not position(yellow,16,16,t), t==5.
%:- not position(red,3,1,t), t==5.
:- not go(red,south,t), t==6.
%:- not position(blue,1,16,t), t==6.
%:- not position(green,16,1,t), t==6.
%:- not position(yellow,16,16,t), t==6.
%:- not position(red,3,15,t), t==6.

#program base.

instance(15).

position(red,1,1,0).
position(blue,1,dimension,0).
position(green,dimension,1,0).
position(yellow,dimension,dimension,0).

barrier( 2, 1,east).
barrier(10, 1,east).
barrier( 4, 2,east).
barrier(14, 2,east).
barrier( 2, 3,east).
barrier( 11,3,east).
barrier( 7, 4,east).
barrier( 3, 7,east).
barrier( 14,7,east).
barrier( 7, 8,east). % middle
barrier( 10,8,west). % middle
barrier( 11,8,east).
barrier( 7, 9,east). % middle
barrier( 10,9,west). % middle
barrier( 4,10,east).
barrier( 2,11,east).
barrier( 8,11,east).
barrier(13,11,east).
barrier(11,12,east).
barrier(14,13,east).
barrier( 6,14,east).
barrier(3, 15,east).
barrier(10,15,east).
barrier(4, 16,east).
barrier(12,16,east).

barrier( 5, 1,south).
barrier(15, 1,south).
barrier( 2, 2,south).
barrier(12, 3,south).
barrier( 7, 4,south).
barrier(16, 4,south).
barrier( 1, 6,south).
barrier( 4, 7,south).
barrier( 8, 7,south). % middle
barrier( 9, 7,south). % middle
barrier(11, 7,south).
barrier(14, 7,south).
barrier(16, 9,south).
barrier( 2,10,south).
barrier( 5,10,south).
barrier( 8,10,north). % middle
barrier( 9,10,north). % middle
barrier( 9,10,south).
barrier(14,10,south).
barrier( 1,12,south).
barrier(11,12,south).
barrier( 7,13,south).
barrier(15,13,south).
barrier(10,14,south).
barrier( 3,15,south).

