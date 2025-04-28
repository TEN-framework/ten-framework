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
    edges = {}
    cover = {}
    vertices = {}
    for key,atom in ipairs(self.last) do
        if atom.name == "edge" then
            args = atom.arguments
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
    pre:load("instance.lp")
    pre:load("preprocess.lp")
    pre:ground({{"base", {}}})
    local ret = pre:solve{on_model=function(...) self:on_model(...) end}
    local k = 0
    while ret.satisfiable do
        k = k + 1
        pre:ground({{"step", {k}}}, self)
        ret = pre:solve{on_model=function(...) self:on_model(...) end}
    end
    self:prepare_instance(k)
    prg:load("cover.lp")
    prg:ground({{"base", {}}}, self)
    prg:solve()
end

function main(prg)
    local s = State.new(prg)
    s:run(prg)
end

#end.
