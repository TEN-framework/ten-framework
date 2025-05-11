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

#show push_from/4.
#show push_to/4.

#program base.

% convert my instances
square(X, Y)        :- field(X, Y).
target_square(X, Y) :- target(X, Y).
initial_box(X, Y)   :- stone(X, Y).
initial_at(X, Y)    :- start(X, Y).

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

% reachable places at timestep 0
route(X, Y, 0) :- initial_at(X, Y).
route(X, Y, 0) :- dir(D, DX, DY), route(X + DX, Y + DY, 0), not box(X, Y, 0), square(X, Y).

% box positions at timestep 0
box(X, Y, 0) :- initial_box(X, Y).

#program cumulative(k).

% reachable places at timestep k
route(X, Y, k) :- at(X, Y, k), square(X, Y).
route(X, Y, k) :- dir(D, DX, DY), route(X + DX, Y + DY, k), not box(X, Y, k), square(X, Y).

% select a place to push from (must be reachable and have a box to push)
1 { push_from(X, Y, D, k) : square(X, Y), direction(D) } 1.
 :- push_from(X, Y, D, k), not route(X, Y, k - 1), square(X, Y), direction(D).
 :- push_from(X, Y, D, k), not box(X + DX, Y + DY, k - 1), square(X, Y), dir(D, DX, DY).

% push the box 1 or more fields into direction D (but at least one)
push(X, Y, D, k) :- push_from(X, Y, D, k).
{ push(X + DX, Y + DY, D, k) } :- push(X, Y, D, k), square(X, Y), dir(D, DX, DY).
 :- push(X, Y, D, k), not square(X + 2 * DX, Y + 2 * DY), dir(D, DX, DY).
 :- push(X, Y, D, k), box(X + 2 * DX, Y + 2 * DY, k - 1), dir(D, DX, DY).

% the last push is the final location (one could try to encode this using a min???)
push_to(X + DX, Y + DY, D, k) :- push(X, Y, D, k), not push(X + DX, Y + DY, D, k), dir(D, DX, DY).

% the player stands behind the pushed box
at(X, Y, k) :- push_to(X, Y, D, k).

% put the pushed box at the new location
box(X + DX, Y + DY, k) :- push_to(X, Y, D, k), dir(D, DX, DY).
% all other boxes stay where they are
box(X, Y, k) :- box(X, Y, k - 1), square(X, Y), not push_from(X - DX, Y - DY, D, k) : dir(D, DX, DY).

% assert redundant moves
 :- push_to(X, Y, D, k - 1), push_from(X, Y, D, k).

#external volatile(k).

% the goal :)
 :- target_square(X, Y), not box(X, Y, k), volatile(k).

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
