#script (python)

from clingo import Control, Number, Tuple_
import sys

class State:
    def __init__(self):
        self.leaves = {}
        self.last = None

    def fix(self, k):
        return self.leaves.get(k.number-1, [])

    def on_model(self, m):
        self.last = m.symbols(atoms=True)
        for atom in self.last:
            if atom.name == "leaf":
                args = atom.arguments
                self.leaves[args[0].number] = args[1]

    def prepare_instance(self, k):
        edges = []
        cover = []
        vertices = set()
        for atom in self.last:
            if atom.match("edge", 3):
                args = atom.arguments
                if args[0].number == k-1:
                    edges.append(Tuple_((args[1], args[2])))
                    vertices.add(args[1])
                    vertices.add(args[2])
            if atom.name == "cover":
                cover.append(atom.arguments[1])
        vertices = list(vertices)
        setattr(self, "edge", lambda: edges)
        setattr(self, "cover", lambda: cover)
        setattr(self, "vertex", lambda: vertices)

    def run(self, prg):
        pre = Control()
        pre.load("instance.lp")
        pre.load("preprocess.lp")
        pre.ground([("base", [])])
        ret = pre.solve(on_model=self.on_model)
        k = 0
        while ret.satisfiable:
            k = k + 1
            pre.ground([("step", [Number(k)])], self)
            ret = pre.solve(on_model=self.on_model)
        self.prepare_instance(k)
        prg.load("cover.lp")
        prg.ground([("base", [])], self)
        prg.solve()

def main(prg):
    s = State()
    s.run(prg)

#end.
