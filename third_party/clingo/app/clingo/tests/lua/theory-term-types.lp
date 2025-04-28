#script (lua)
clingo = require("clingo")

function types() 
    return {
        tostring(clingo.TheoryTermType.Tuple),
        tostring(clingo.TheoryTermType.List),
        tostring(clingo.TheoryTermType.Set),
        tostring(clingo.TheoryTermType.Function),
        tostring(clingo.TheoryTermType.Symbol),
        tostring(clingo.TheoryTermType.Number),
    }
end

#end.

p(@types()).

