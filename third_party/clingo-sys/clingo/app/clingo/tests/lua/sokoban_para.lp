#script (lua)

function main(prg)
    step = 1
    while true do
        parts = {}
        if step > 1 then
            prg:release_external(clingo.Function("volatile", {step-1}))
            prg:cleanup()
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

#show push_box/4.
#show push_to/4.
#show at/3.

#program base.

% possible directions
direction(u).
direction(d).
direction(r).
direction(l).

% possible moves
dir(u,  0,  1).
dir(d,  0, -1).
dir(r,  1,  0).
dir(l, -1,  0).

% box positions at timestep 0
box(X, Y, 0) :- initial_box(X, Y).

% player at timestep 0
at(X, Y, 0) :- initial_at(X, Y).

#program cumulative(k).

% reachable places at timestep k
routePre(X, Y, k) :- at(X, Y, k - 1), square(X, Y).
routePre(X, Y, k) :- dir(D, DX, DY), routePre(X + DX, Y + DY, k), not box(X, Y, k - 1), square(X, Y).

routePost(X, Y, k) :- at(X, Y, k), square(X, Y).
routePost(X, Y, k) :- dir(D, DX, DY), routePost(X + DX, Y + DY, k), not box(X, Y, k - 1), not box(X, Y, k), square(X, Y).

% move the player (this could be restricted to moves behind boxes)
1 { at(X, Y, k) : square(X, Y) } 1.
 :- not routePre(X, Y, k), at(X, Y, k).
 :- box(X, Y, k), at(X, Y, k).

% select at least one box to push into exactly one direction (must be reachable and have a box to push)
1 { push_box(X, Y, D, k) : square(X, Y), direction(D) }.
 :- push_box(X, Y, D, k), not routePre(X - DX, Y - DY, k), dir(D, DX, DY).
 :- push_box(X, Y, D, k), not routePost(X - DX, Y - DY, k), dir(D, DX, DY).
 :- push_box(X, Y, D, k), not box(X, Y, k - 1).
 :- push_box(X, Y, D1, k), push_box(X, Y, D2, k), D1 != D2.

% push the box 1 or more fields into direction D (but at least one)
push(X + DX, Y + DY, D, k) :- push_box(X, Y, D, k), dir(D, DX, DY).
{ push(X + DX, Y + DY, D, k) } :- push(X, Y, D, k), square(X, Y), dir(D, DX, DY).
 :- push(X, Y, D, k), not square(X, Y).
 :- push(X, Y, D, k), box(X, Y, k - 1).

% the last push is the final location
push_to(X, Y, D, k) :- push(X, Y, D, k), not push(X + DX, Y + DY, D, k), dir(D, DX, DY).
 :- push_to(X, Y, D1, k), push_to(X, Y, D2, k), D1 != D2.

% put the pushed box at the new location
box(X, Y, k) :- push_to(X, Y, D, k).
% all other boxes stay where they are
box(X, Y, k) :- box(X, Y, k - 1), square(X, Y), not push_box(X, Y, D, k) : direction(D).

% assert redundant moves
% :- push_box(X, Y, D, k - 1), push_from(X, Y, D, k).

#external volatile(k).
% the goal :)
 :- target_square(X, Y), not box(X, Y, k), volatile(k).

% restrict search space a little
:- not at(1,1,k), k==0.
:- not at(1,1,k), k==6.
:- not at(1,1,k), k==8.
:- not at(7,2,k), k==7.
:- not at(1,3,k), k==1.
:- not at(1,3,k), k==3.
:- not at(2,3,k), k==5.
:- not at(3,3,k), k==2.
:- not at(5,4,k), k==9.
:- not at(5,5,k), k==4.
:- not push_box(3,2,u,k), k==7.
:- not push_box(5,2,r,k), k==1.
:- not push_box(4,3,r,k), k==2.
:- not push_box(5,2,r,k), k==3.
:- not push_box(2,3,r,k), k==3.
:- not push_box(5,2,r,k), k==5.
:- not push_box(2,1,r,k), k==8.
:- not push_box(3,2,d,k), k==4.
:- not push_box(5,3,d,k), k==9.
:- not push_box(6,2,l,k), k==2.
:- not push_box(6,2,l,k), k==4.
:- not push_box(3,1,l,k), k==5.
:- not push_box(6,2,l,k), k==6.
:- not push_box(5,2,l,k), k==10.
:- not push_box(4,3,l,k), k==10.
:- not push_to(3,4,u,k), k==7.
:- not push_to(6,2,r,k), k==1.
:- not push_to(5,3,r,k), k==2.
:- not push_to(6,2,r,k), k==3.
:- not push_to(4,3,r,k), k==3.
:- not push_to(6,2,r,k), k==5.
:- not push_to(3,1,r,k), k==8.
:- not push_to(3,1,d,k), k==4.
:- not push_to(5,2,d,k), k==9.
:- not push_to(5,2,l,k), k==2.
:- not push_to(5,2,l,k), k==4.
:- not push_to(2,1,l,k), k==5.
:- not push_to(3,2,l,k), k==6.
:- not push_to(3,2,l,k), k==10.
:- not push_to(3,3,l,k), k==10.

#program base.

initial_at(1,1).
square(1, 1).
square(2, 1).
square(3, 1).
target_square(3, 1).
square(4, 1).
square(1, 2).
square(3, 2).
initial_box(3, 2).
target_square(3, 2).
square(4, 2).
square(5, 2).
initial_box(5, 2).
square(6, 2).
square(7, 2).
square(1, 3).
square(2, 3).
initial_box(2, 3).
square(3, 3).
target_square(3, 3).
square(4, 3).
initial_box(4, 3).
square(5, 3).
square(7, 3).
square(3, 4).
target_square(3, 4).
square(5, 4).
square(6, 4).
square(7, 4).
square(3, 5).
square(4, 5).
square(5, 5).
