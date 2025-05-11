% show instances of selected predicates only
#show.

% output stack representation
#show order/4.

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% program part for generating states providing relative block positions, using %
% - parameter k for block number                                               %
% - parameter t for step number at instantiation                               %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#program state(k,t).

% a block's scope includes the table (labeled 0) and blocks with smaller numbers
scope(k,0,t).
scope(k,I,t) :- scope(I,0,t), I < k.

% order(1,k,I,t) provides the closest block I in scope above k, if any at time t
#count{ I : order(1,k,I,t) : scope(k,I,t), 0 < I } 1.

% order(-1,k,I,t) provides the closest block/table I in scope below k at time t
1 #count{ I : order(-1,k,I,t) : scope(k,I,t) } 1.

% trace order/4 to derive transitive closure of objects in scope above/below k
trans(D,k,I,t) :- order(D,k,I,t).
trans(D,k,I,t) :- trans(D,k,J,t), order(D,J,I,t).
trans(D,k,J,t) :- trans(D,k,I,t), order(-D,J,I,t), scope(k,J,t).

% state constraint that the table is transitively below block k (redundant)
 :- not trans(-1,k,0,t).

% state constraint propagating immediate above/below relationships
 :- order(-1,k,I,t), order(1,k,J,t), not order(-1,J,I,t), not order(1,I,J,t).

% state constraint aligning immediate and transitive above/below relationships
 :- order(-D,k,J,t), order(D,J,I,t), not trans(D,k,I,t), 0 < I. % D=-1 redundant

% state constraint directing transitive above/below relationships on convergence
 :- order(D,k,I,t), order(D,J,I,t), not trans(-D,k,J,t), scope(k,J,t), 0 < I.

% rules below are volatile with life span 1
% #program volatile(k,t).

#external scope(k,0,t+1).

% goal state conditions
 :- goal_above(k,I), not trans(-1,k,I,t), not scope(k,0,t+1).
 :- goal_below(k,I), not trans(1,k,I,t), not scope(k,0,t+1).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% program part for generating moves having a block as object and target, using %
% - parameter k for block number                                               %
% - parameter t for step number at instantiation                               %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#program move(k,t).

% external atoms indicating involvement of blocks with greater numbers in move
#external change(-1,k,I,t) : scope(I,0,t), I <= k.
#external change( 1,k,I,t) : scope(I,0,t-1), I <= k.
#external change(-1,k,t).
#external change( 1,k,t).

% output object and target block of move
#show object(k,t) : object(k,t).
#show target(k,t) : target(k,t).

% propagate role as move object (via D=1) or target (via D=-1) to smaller block
change(1,k-1,I,t) :- order(1,k,I,t), scope(I,0,t-1), not order(1,k,I,t-1).
change(D,k-1,I,t) :- order(D,k,I,t-(D+|D|)/2), scope(k,0,t-1), 0 < I,
                     not order(D,k,I,t+(D-|D|)/2).
change(D,k-1,I,t) :- change(D,k,I,t), I < k. % , scope(I,0,t-(D+|D|)/2).

% combine possibilities how move affects block as object (D=1) or target (D=-1)
affect(-D,k,t) :- order(D,k,I,t+(D-|D|)/2), scope(I,0,t-1) : D == 1;
                  not order(D,k,I,t-(D+|D|)/2).
affect( D,k,t) :- change(D,k,k,t). % , scope(k,0,t-(D+|D|)/2).

% propagate existence of some move object or target to smaller blocks and table
change(D,k-1,t) :- affect(D,k,t).
change(D,k-1,t) :- change(D,k,t).

% derive moved object and corresponding target
object(k,t) :- affect(1,k,t).
target(k,t) :- affect(-1,k,t), not change(-1,k,t).

% moved object must be unique
 :- object(k,t), change(1,k,t).

% moved object must be free
 :- object(k,t), order(1,k,_,t-1).
 :- object(I,t), order(-1,k,I,t-1).

% moved object must not go under (unmoved) pre-existing block
 :- object(k,t), order(1,k,I,t), scope(I,0,t-1).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% program part for deriving target table from move without target block, using %
% - parameter t for step number at instantiation                               %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#program table(t).

% external atoms indicating involvement of blocks with greater numbers in move
#external change(-1,0,t).
#external change( 1,0,t).

% output target table of move
#show target(0,t) : target(0,t).

% derive table as target of move
target(0,t) :- change(1,0,t), not change(-1,0,t).

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% program part for establishing consistent initial position for a block, using %
% - parameter k for block number                                               %
% - parameter t for step number at instantiation                               %
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#program init(k,t).

% no appearance under unmoved pre-existing block
 :- order(1,k,I,t), scope(I,0,t-1), not object(I,t).

% initial state conditions
 :- init_above(k,I), not trans(-1,k,I,t).
 :- init_below(k,I), not trans(1,k,I,t).
% clingo-4-banane clingo-4-blocksworld.lp clingo-4-main.lp 0

#script (lua)

function main(prg)
    parts = {}
    parts[#parts+1] = {"init", {1, 0}}
    parts[#parts+1] = {"state", {1, 0}}
    prg:cleanup()
    prg:ground(parts)
	prg:solve()

    parts = {}
    parts[#parts+1] = {"base_2", {}}
    parts[#parts+1] = {"init", {2,0}}
    parts[#parts+1] = {"state", {2,0}}
	for i=1,2 do
        parts[#parts+1] = {"table", {i}}
		for j=1,2 do
            parts[#parts+1] = {"state", {j,i}}
		    parts[#parts+1] = {"move", {j,i}}
        end
    end
    prg:cleanup()
    prg:ground(parts)
	prg:solve()

    parts = {}
	parts[#parts+1] = {"base_3", {}}
	parts[#parts+1] = {"init", {3,0}}
	parts[#parts+1] = {"state", {3,0}}
	for i=1,3 do
		parts[#parts+1] = {"state", {3,i}}
		parts[#parts+1] = {"move", {3,i}}
    end
	for j=1,2 do
		parts[#parts+1] = {"state", {j,3}}
		parts[#parts+1] = {"move", {j,3}}
    end
	parts[#parts+1] = {"table", {3}}
    prg:cleanup()
    prg:ground(parts)
	prg:solve()

    parts = {}
	parts[#parts+1] = {"base_4", {}}
	parts[#parts+1] = {"init", {4,0}}
	parts[#parts+1] = {"state", {4,0}}
	for i=1, 4 do
		parts[#parts+1] = {"state", {4,i}}
		parts[#parts+1] = {"move", {4,i}}
    end
	for j = 1, 3 do
		parts[#parts+1] = {"state", {j,4}}
		parts[#parts+1] = {"move", {j,4}}
    end
	parts[#parts+1] = {"table", {4}}
    prg:cleanup()
    prg:ground(parts)
	prg:solve()

    parts = {}
	parts[#parts+1] = {"base_5", {}}
	parts[#parts+1] = {"init", {5,1}}
	for i=1, 7 do
		parts[#parts+1] = {"state", {5,i}}
		parts[#parts+1] = {"move", {5,i}}
    end
	for i=5, 7 do
		parts[#parts+1] = {"table", {i}}
		for j=1, 4 do
			parts[#parts+1] = {"state", {j,i}}
			parts[#parts+1] = {"move", {j,i}}
        end
    end
    prg:cleanup()
    prg:ground(parts)
	prg:solve()
end
#end.

#program base_2.

init_above(2,1).
goal_below(2,1).

#program base_3.

init_above(3,2).
goal_below(3,1).

#program base_4.

init_above(4,2).
init_below(4,3).
goal_above(4,3).
goal_below(4,1).

#program base_5.

init_above(5,1).
init_below(5,3).
goal_above(5,2).
