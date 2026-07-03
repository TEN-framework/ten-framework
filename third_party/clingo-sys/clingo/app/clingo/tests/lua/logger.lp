#script (lua)

clingo = require("clingo")

messages = {}

function log(code, message)
    rep, _ = message:gsub(" ", "_")
    messages[#messages + 1] = (clingo.Tuple{tostring(code), rep})
end

function main(prg)
    local ctl = clingo.Control({}, log, 2)
    ctl:add("base", {}, ":- a. :- b. :- c.")
    ctl:ground({{"base", {}}})

    prg:ground({{"base", {}}})
    prg:solve()
end

function msg()
    return messages
end

#end.

msg(X,Y) :- (X, Y) = @msg().
