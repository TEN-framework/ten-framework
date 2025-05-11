#script (lua)

require("clingo")

Values = { }
Values.__index = Values

function Values.new(val)
    local x = { value = val }
    setmetatable(x, Values)
    return x
end

function Values:get()
    return clingo.Tuple(self.value)
end

function ground(prg, solve)
    ctl = clingo.Control()
    ctl:add("base", {}, prg)
    ctl:ground({{"base", {}}})
    if solve then
        ctl:solve()
    end
    return ctl.is_conflicting
end

function main(prg)
    values = {
        tostring(ground("a.")),
        tostring(ground(":-.")),
        tostring(ground("2 { a; b }. :- a, b.")),
        tostring(ground("2 { a; b }. :- a, b.", true))
    }

    prg:ground({{"base", {}}}, Values.new(values))
    prg:solve()
end

#end.

p(X) :- X = @get().
