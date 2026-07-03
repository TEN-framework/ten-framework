#script (python)

import clingo
import gc
import weakref

class Propagator:
    def __init__(self, x):
        self.x = x

    def init(self, init):
        pass

class Main:
    def __init__(self):
        self.ctl = clingo.Control()
        self.ctl.register_propagator(Propagator(self))

def main(prg):
    m1 = Main()
    r1 = weakref.ref(Main())
    r2 = weakref.ref(Main())
    gc.collect()
    freed = 0
    freed += r1() is None
    freed += r2() is None
    prg.add("base", ["k"], "p(k).")
    prg.ground([('base', [clingo.Number(freed)])])
    prg.solve()

#end.
