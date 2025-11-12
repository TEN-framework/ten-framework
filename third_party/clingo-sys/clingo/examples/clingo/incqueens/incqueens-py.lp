#script(python)

from clingo import Number

def main(prg):
    n = 0
    parts = []
    for arg in prg.get_const("calls").arguments:
        lower = arg.arguments[0].number
        upper = arg.arguments[1].number
        while n < upper:
            n += 1
            parts.append(("board", [Number(n)]))
            if n >= lower:
                prg.ground(parts)
                parts = []
                print('SIZE {0}'.format(n))
                prg.solve()

#end.
