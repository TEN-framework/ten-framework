#script (lua)

function main(prg)
    step = 1
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
        if ret.satisfiable or step == 13 then
            break
        end
        step = step + 1
    end
end

#end.

#show jump/4.
#const n=31.

#program base.

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

% the field
field(0, 0).
field(X, Y) :- stone(X, Y).

% the 3x3 quads
stone(-1 ..  1, -3 .. -1).
stone(-3 .. -1, -1 ..  1).
stone( 1 ..  3, -1 ..  1).
stone(-1 ..  1,  1 ..  3).

dir(west;east;north;south).

dir(west , -1,  0).
dir(east ,  1,  0).
dir(north,  0, -1).
dir(south,  0,  1).

stone(X, Y, 0) :- stone(X, Y).

% possible jumps
field(X, Y, D) :- field(X, Y), field(X + 2 * DX, Y + 2 * DY), dir(D, DX, DY).

#program cumulative(t).

% fix the first jump to avoid symmetries
jump(0,-2,south,t) :- t == 1.

% choose one stone
1 { jump(X, Y, D, t) : field(X, Y, D)  }.

% fail if no stone at position
 :- jump(X, Y, D, t), not stone(X, Y, t - 1).
% fail if stone at target
 :- jump(X, Y, D, t), stone(X + 2 * DX, Y + 2 * DY, t - 1), dir(D, DX, DY).
% fail if no stone to jump over
 :- jump(X, Y, D, t), not stone(X + DX, Y + DY, t - 1), dir(D, DX, DY).

% a stone may not jump in two directions
 :- jump(X, Y, D1, t), jump(X, Y, D2, t), D1 != D2.
% fail if two stones jumped over the same location
 :- jump(X1, Y1, D1, t), jump(X2, Y2, D2, t), dir(D1, DX1, DY1), dir(D2, DX2, DY2), X1 + DX1 == X2 + DX2, Y1 + DY1 == Y2 + DY2, D1 != D2.
% fail if two stones jumped at the same target
 :- jump(X1, Y1, D1, t), jump(X2, Y2, D2, t), dir(D1, DX1, DY1), dir(D2, DX2, DY2), X1 + 2 * DX1 == X2 + 2 * DX2, Y1 + 2 * DY1 == Y2 + 2 * DY2, D1 != D2.
% a removed stone may not jump
 :- jump(X, Y, D, t), removed(X, Y, t).

% place the new stone
stone(X + 2 * DX, Y + 2 * DY, t) :- jump(X, Y, D, t), dir(D, DX, DY).
% mark the moved stone
jumped(X, Y, t) :- jump(X, Y, D, t).
% remove the stone
removed(X + DX, Y + DY, t) :- jump(X, Y, D, t), dir(D, DX, DY).
% keep other stones
stone(X, Y, t) :- field(X, Y), stone(X, Y, t - 1), not jumped(X, Y, t), not removed(X, Y, t).

% restrict search space a little
:- not jump(0,-2,south,t), t==1.
:- not jump(0,-1,east,t), t==4.
:- not jump(-1,1,east,t), t==4.
:- not jump(-2,-1,east,t), t==5.
:- not jump(-1,-3,east,t), t==6.
:- not jump(-3,0,east,t), t==7.
:- not jump(-3,1,east,t), t==7.
:- not jump(-3,-1,east,t), t==8.
:- not jump(0,1,east,t), t==10.
:- not jump(-1,2,east,t), t==11.
:- not jump(1,-3,south,t), t==5.
:- not jump(1,-3,south,t), t==7.
:- not jump(-1,0,south,t), t==8.
:- not jump(-1,-2,south,t), t==9.
:- not jump(3,-1,south,t), t==9.
:- not jump(0,-1,south,t), t==9.
:- not jump(-1,0,south,t), t==10.
:- not jump(2,-1,west,t), t==2.
:- not jump(0,-1,west,t), t==7.
:- not jump(2,-1,west,t), t==8.
:- not jump(3,1,west,t), t==11.
:- not jump(2,0,west,t), t==13.
:- not jump(1,1,north,t), t==3.
:- not jump(-1,3,north,t), t==5.
:- not jump(1,2,north,t), t==5.
:- not jump(1,0,north,t), t==6.
:- not jump(-1,1,north,t), t==6.
:- not jump(-1,3,north,t), t==9.
:- not jump(1,2,north,t), t==12.

#external volatile(t).

% there shall be at least n free places
 :- { not stone(X, Y, t) : field(X, Y) } n, volatile(t).
% the last stone shall be in the center
 :- not stone(0, 0, t), volatile(t).

