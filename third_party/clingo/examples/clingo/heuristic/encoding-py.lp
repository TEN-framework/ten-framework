#script (python)

import clingo

class State:
    def __init__(self):
        self.degree = {}

class ColoringHeuristic:
    def __init__(self):
        self.graph = {}
        self.assign = {}
        self.states = {}

    def init(self, init):
        self.states = [ State() for _ in range(init.number_of_threads) ]
        for a in init.symbolic_atoms:
            if a.match("edge", 2):
                u = a.symbol.arguments[0]
                v = a.symbol.arguments[1]
                self.graph.setdefault(u, []).append(v)
                self.graph.setdefault(v, []).append(u)
                for state in self.states:
                    state.degree.setdefault(u, 0)
                    state.degree.setdefault(v, 0)
                    state.degree[u] += 1
                    state.degree[v] += 1
            elif a.match("assign", 2):
                u = a.symbol.arguments[0]
                l = init.solver_literal(a.literal)
                init.add_watch(l)
                self.assign.setdefault(l, []).append(u)

    def propagate(self, ctl, changes):
        state = self.states[ctl.thread_id]
        for l in changes:
            for u in self.assign[l]:
                for v in self.graph[u]:
                    state.degree[v] -= 1

    def undo(self, thread_id, assignment, changes):
        state = self.states[thread_id]
        for l in changes:
            for u in self.assign[l]:
                for v in self.graph[u]:
                    state.degree[v] += 1

    def decide(self, thread_id, assignment, fallback):
        # in practice this implementation is unusable because it is too inefficient
        # a heap should be used to extract the vertices with maximum degree
        state = self.states[thread_id]
        decision, degree = 0, 0
        for l, vertices in self.assign.items():
            for u in vertices:
                if assignment.value(l) is None and degree < state.degree[u]:
                    decision, degree = l, state.degree[u]
        return decision

def main(prg):
    prg.register_propagator(ColoringHeuristic())
    prg.ground([("base", [])])
    ret = prg.solve()

#end.

1 { assign(U,C) : color(C) } 1 :- vertex(U).
:- edge(U,V), assign(U,C), assign(V,C).

