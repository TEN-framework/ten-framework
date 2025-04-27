#script (lua) 

State = { }
State.__index = State

function State.new(val)
    local x = { value = val }
    setmetatable(x, State)
    return x
end

function State:test(value)
    return self.value + value.number
end

function State:on_model(m)
    print(tostring(m))
end

function State:run(prg)
    prg:ground({{"base", {}}}, self)
    prg:solve{on_model=function (...) self:on_model(...) end}
end

function main(prg) 
    x = State.new(21)
    x:run(prg)
end

#end.
