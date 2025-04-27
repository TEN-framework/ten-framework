#script (lua)

clingo = require("clingo")

State = { }
State.__index = State

function State.new()
    local x = { 
        leaves = {},
        last   = nil
    }
    setmetatable(x, State)
    return x
end

function State:fix(k)
    local leaf = self.leaves[k.number-1]
    if leaf then
        return leaf
    else
        return {}
    end
end

function State:on_model(m)
    self.last = m:symbols{atoms=true}
    for key,atom in ipairs(self.last) do
        if atom.name == "leaf" then
            local args = atom.arguments
            self.leaves[args[1]] = args[2]
        end
    end
end

function State:prepare_instance(k)
    local edges = {}
    local cover = {}
    local vertices = {}
    for key,atom in ipairs(self.last) do
        if atom.name == "edge" then
            local args = atom.arguments
            if args[1].number == k-1 then
                edges[#edges + 1] = clingo.Tuple{args[2], args[3]}
                vertices[#vertices + 1] = args[2]
                vertices[#vertices + 1] = args[3]
            end
        end
        if atom.name == "cover" then
            cover[#cover + 1] = atom.arguments[2]
        end
    end
    self.edge = function (self) return edges end
    self.vertex = function (self) return vertices end
    self.cover = function (self) return cover end
end

function State:run(prg)
    local pre = clingo.Control()
    pre:add("base", {}, "\
edge(a,(b;c)).\n\
edge(b,(c;d)).\n\
edge(d,e).\n\
edge(e,f).\n\
\n\
edge(x,(a;c)). % edge to keep something to solve\n\
\n\
edge(X,Y) :- edge(Y,X).\n\
vertex(X) :- edge(X,Y;Y,X).\n\
\n\
edge(0,X,Y) :- edge(X,Y).\n")
    pre:add("step", {"k"}, "\
:- not leaf(k-1,@fix(k)).\n\
\n\
1 { leaf(k,X) : vertex(X) } 1.\n\
cover(k,X) :- edge(k-1,X,Y), leaf(k,Y).\n\
edge(k,X,Y) :- edge(k-1,X,Y), not cover(k,X), not cover(k,Y).\n\
\n\
:- edge(k-1,X,Y), leaf(k,Y), edge(k-1,Y,Z), X < Z.\n\
:- leaf(k,Y), not edge(k-1,_,Y).\n")
    pre:ground({{"base", {}}}, self)
    local ret = pre:solve{on_model=function(...) self:on_model(...) end}
    local k = 0
    while ret.satisfiable do
        k = k + 1
        pre:cleanup()
        pre:ground({{"step", {k}}}, self)
        ret = pre:solve{on_model=function(...) self:on_model(...) end}
    end
    self:prepare_instance(k)
    prg:add("base", {}, "\
vertex(X) :- X = @vertex().\n\
edge(X,Y) :- (X,Y) = @edge().\n\
cover(X)  :- X = @cover().\n\
\n\
{ cover(X) : vertex(X) }.\n\
:- edge(X,Y), not cover(X), not cover(Y).\n\
%#minimize { 1,X : cover(X) }.\n\
\n\
#show cover/1.\n")
    prg:ground({{"base", {}}}, self)
    prg.configuration.solve.models = 0
    prg:solve()
end

function main(prg)
    local s = State.new(prg)
    s:run(prg)
end

#end.
