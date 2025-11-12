#script (python)

from clingo import Function, Tuple_, Number, String

class Observer:
    def __init__(self):
        self.__delayed = []
        self.__symbols = {}
        self.__reified = []
        self.__terms   = {}
        self.__elems   = {}

    def __getattr__(self, name):
        assert(not name.startswith("_"))
        def caller(*args):
            self.__delayed.append((name, args))
        return caller

    def __map(self, lit):
        sign = False
        if lit < 0:
            sign = True
            lit = -lit
        ret = self.__symbols.get(lit, Function("__aux"))
        if sign:
            ret = Function("neg", [ret])
        return ret

    def init_program(self, incremental):
        self.__reified.append(Function("init_program", [Number(incremental)]))

    def begin_step(self):
        self.__reified.append(Function("begin_step", []))

    def _rule(self, choice, head, body):
        head = sorted(set([ self.__map(atm) for atm in head ]))
        body = sorted(set([ self.__map(lit) for lit in body ]))
        self.__reified.append(Function("rule", [Number(choice), Tuple_(head), Tuple_(body)]))

    def _weight_rule(self, choice, head, lower_bound, body):
        head = sorted(set([ self.__map(atm) for atm in head ]))
        body = sorted(set([ Tuple_([self.__map(lit), Number(weight)]) for lit, weight in body ]))
        self.__reified.append(Function("weight_rule", [Number(choice), Tuple_(head), Number(lower_bound), Tuple_(body)]))

    def _minimize(self, priority, literals):
        literals = sorted(set([ Tuple_([self.__map(lit), Number(weight)]) for lit, weight in literals ]))
        self.__reified.append(Function("minimize", [Number(priority), Tuple_(literals)]))

    def _project(self, atoms):
        atoms = sorted(set([ self.__map(atm) for atm in atoms ]))
        self.__reified.append(Function("project", [Tuple_(atoms)]))

    def output_atom(self, symbol, atom):
        self.__symbols[atom] = symbol
        self.__reified.append(Function("output_atom", [symbol]))

    def _output_term(self, symbol, condition):
        condition = sorted(set([ self.__map(lit) for lit in condition ]))
        self.__reified.append(Function("output_term", [symbol, Tuple_(condition)]))

    def _output_csp(self, symbol, value, condition):
        condition = sorted(set([ self.__map(lit) for lit in condition ]))
        self.__reified.append(Function("output_csp", [symbol, Number(value), Tuple_(condition)]))

    def _external(self, atom, value):
        self.__reified.append(Function("external", [self.__map(atom), String(str(value).replace('TruthValue.', ''))]))

    def _assume(self, literals):
        literals = sorted(set([ self.__map(lit) for lit in literals ]))
        self.__reified.append(Function("assume", [Tuple_(literals)]))

    def _heuristic(self, atom, type, bias, priority, condition):
        condition = sorted(set([ self.__map(lit) for lit in condition ]))
        self.__reified.append(Function("heuristic", [self.__map(atom), String(str(type).replace('HeuristicType.', '')), Number(bias), Tuple_(condition)]))

    def _acyc_edge(self, node_u, node_v, condition):
        condition = sorted(set([ self.__map(lit) for lit in condition ]))
        self.__reified.append(Function("acyc_edge", [Number(node_u), Number(node_v), Tuple_(condition)]))

    def theory_term_number(self, term_id, number):
        self.__terms[term_id] = lambda: Number(number)

    def theory_term_string(self, term_id, name):
        self.__terms[term_id] = lambda: Function(name)

    def theory_term_compound(self, term_id, name_id_or_type, arguments):
        self.__terms[term_id] = lambda: Function(self.__terms[name_id_or_type]().name, [self.__terms[i]() for i in arguments])

    def theory_element(self, element_id, terms, condition):
        self.__elems[element_id] = lambda: Function("elem", [Tuple_([self.__terms[i]() for i in terms]), Tuple_(sorted(set([ self.__map(lit) for lit in condition ])))])

    def _theory_atom(self, atom_id_or_zero, term_id, elements):
        self.__symbols[atom_id_or_zero] = Function("theory", [self.__terms[term_id](), Tuple_(sorted(set([ self.__elems[e]() for e in elements ])))]);

    def _theory_atom_with_guard(self, atom_id_or_zero, term_id, elements, operator_id, right_hand_side_id):
        self.__symbols[atom_id_or_zero] = Function("theory", [self.__terms[term_id](), Tuple_(sorted(set([ self.__elems[e]() for e in elements ]))), self.__terms[operator_id](), self.__terms[right_hand_side_id]()]);

    def end_step(self):
        self.__reified.append(Function("end_step", []))

    def finalize(self):
        for name, args in self.__delayed:
            if name.startswith("theory_atom"):
                getattr(self, "_" + name)(*args)
        for name, args in self.__delayed:
            if not name.startswith("theory_atom"):
                getattr(self, "_" + name)(*args)
        return Context(self.__reified)

class Context:
    def __init__(self, reified):
        self.__reified = reified[:]

    def get(self):
        return self.__reified

def main(prg):
    obs = Observer()
    prg.register_observer(obs)
    prg.ground([("base", [])])
    prg.solve(assumptions=[(Function("b"), False)])
    ctx = obs.finalize()
    prg.add("q", [], "q(@get()).")
    prg.ground([("q", [])], ctx)
    prg.solve()

#end.

1 {a; b}.
#minimize {1:a; 2:b}.
#project a.
#show x : a, b.
#external a.
#heuristic a : b. [1@2,sign]
#edge (a,b) : a, b.

#theory t {
  term   { + : 1, binary, left };
  &a/0 : term, any;
  &b/1 : term, {=}, term, any
}.
a :- &a { 1+2,"test": a, b }.
b :- &b(3) { } = 17.
