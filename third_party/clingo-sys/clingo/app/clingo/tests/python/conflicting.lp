#script (python)

from clingo import Control, String, Tuple_

class Values:
    def __init__(self, values):
        self.__values = values
    def get(self):
        return Tuple_(self.__values)

def ground(prg, solve=False):
    ctl = Control()
    ctl.add("base", [], prg)
    ctl.ground([("base", [])])
    if solve: ctl.solve()
    return ctl.is_conflicting

def main(prg):
    values = []
    values.append(String(str(ground("a."))))
    values.append(String(str(ground(":-."))))
    values.append(String(str(ground("2 { a; b }. :- a, b."))))
    values.append(String(str(ground("2 { a; b }. :- a, b.", True))))

    prg.ground([("base", [])], Values(values))
    prg.solve()

#end.

p(X) :- X = @get().
