#script (python)

import clingo

class State:
    def __init__(self):
        self.models = 0

    def on_model(self, m):
        self.models += 1
        m.extend([clingo.Number(self.models)])

def main(prg):
    s = State()
    prg.ground([("base", [])])
    prg.solve(on_model=s.on_model)

#end.

1{a;b;c}1.
