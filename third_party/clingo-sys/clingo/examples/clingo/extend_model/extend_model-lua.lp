#script (lua)

clingo = require("clingo")

State = { }
State.__index = State

function State.new()
    local self = setmetatable({}, State)
    self.models = 0
    return self
end

function State:on_model(m)
    self.models = self.models + 1
    m:extend{clingo.Number(self.models)}
end

function main(prg)
    local s = State.new()
    prg:ground({{"base", {}}})
    prg:solve{on_model=function (m) s:on_model(m) end}
end

#end.

1{a;b;c}1.
