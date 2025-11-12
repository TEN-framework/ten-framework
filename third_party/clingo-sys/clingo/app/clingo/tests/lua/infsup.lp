#script (lua)

clingo = require("clingo")

function inf()
    return clingo.Infimum
end

function sup()
    return clingo.Supremum
end

#end.

inf(@inf()).
sup(@sup()).
