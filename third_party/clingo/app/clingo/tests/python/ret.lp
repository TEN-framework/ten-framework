#script (python)

from clingo import Control, Number, Tuple_

ctl = Control()
def a():
    return Tuple_([Number(1), Number(2), Number(3)])

def b():
    return [Number(1), Number(2), Number(3)]

def c():
    return iter([Number(1), Number(2), Number(3)])

ctl.ground([("base", [])])
ctl.solve()

#end.

a(@a()).
b(@b()).
c(@c()).
