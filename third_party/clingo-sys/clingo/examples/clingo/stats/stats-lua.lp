#script (lua)

json = require("json")

function main(prg)

    prg:ground({{"base", {}}})
    prg:solve()
    print(json.encode(prg.statistics)) -- no pretty printing support

    print("lp.bodies", prg.statistics["problem"]["lp"]["bodies"])
    -- lua tables also support dot access
    print("lp.bodies", prg.statistics.problem.lp.bodies)
    print("optimization", prg.statistics.summary.costs[1])

end

#end.
