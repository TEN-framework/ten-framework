#script (python)

import clingo

def resize(l, n, d = None):
    while len(l) < n:
        l.append(d)

class Propagator:
    class State:
        def __init__(self, pat_len, seq_len):
            self.seq_active = [] # [bool|None]
            self.stack = []      # [+pattern index|-sequence index]
            self.trail = []      # [(decision level,stack index)]
            self.pat = []        # [(pattern literal, item index)|None]
            self.pat_assigned = 0
            resize(self.pat, pat_len)
            resize(self.seq_active, seq_len, True)

    def __init__(self):
        self.__seq_atoms = []     # [(sequence literal, [item index])]
        self.__pat_atoms = dict() # {pattern literal: [(pattern index, item index)]}
        self.__occ_list = []      # [[sequence index]]
        self.__item_map = dict()  # {item: item index}
        self.__pat_len = 0        # the pattern length
        self.__states = []        # [State]

    # {{{1 Initialization Functions

    def __state(self, sid):
        while len(self.__states) <= sid:
            self.__states.append(Propagator.State(self.__pat_len, len(self.__seq_atoms)))
        return self.__states[sid]

    def __map_item(self, item):
        return self.__item_map.setdefault(item, len(self.__item_map))

    def __add_seq_atom(self, init, sid, atom):
        resize(self.__seq_atoms, sid + 1)
        assert(self.__seq_atoms[sid] is None)
        self.__seq_atoms[sid] = (init.solver_literal(atom.literal), [])
        for elem in atom.elements:
            index, item = elem.terms
            index = index.number
            assert(index >= 0)
            item = self.__map_item(str(item))
            resize(self.__seq_atoms[sid][1], index + 1);
            self.__seq_atoms[sid][1][index] = item

    def __add_pat_atoms(self, init, atom):
        for elem in atom.elements:
            lit = init.solver_literal(elem.condition_id)
            index, item = elem.terms
            index = index.number
            assert(index >= 0)
            item = self.__map_item(str(item))
            pat = (index, item)
            if lit not in self.__pat_atoms:
                init.add_watch(lit)
            self.__pat_atoms.setdefault(lit, []).append(pat)
            self.__pat_len = max(self.__pat_len, index + 1)

    def __init_occ_list(self):
        for item in self.__item_map:
            self.__occ_list.append([])
        sid = 0
        for seq in self.__seq_atoms:
            for item in set(seq[1]):
                assert(item is not None)
                self.__occ_list[item].append(sid)
            sid += 1

    def init(self, init):
        for atom in init.theory_atoms:
            term = atom.term
            args = term.arguments
            if term.name == "seq" and len(args) == 1:
                self.__add_seq_atom(init, args[0].number, atom)
            if term.name == "pat" and len(args) == 0:
                self.__add_pat_atoms(init, atom)
        self.__init_occ_list()

    # {{{1 Tracing Functions
    def __str_item(self, iid):
        for item, jid in self.__item_map.iteritems():
            if iid == jid:
                return item

    def __str_items(self, items):
        s = ""
        for item in items:
            s+= self.__str_item(item)
        return s

    def __str_pat(self, state):
        s = ""
        for pat in state.pat:
            s+= "." if pat is None else self.__str_item(pat[1])
        return s

    def __str_seq(self, lit):
        for seq in self.__seq_atoms:
            if seq[0] == lit:
                return self.__str_items(seq[1])

    # {{{1 Propagation

    def __propagate_sequence_lit(self, state, control, sid, lit):
        state.seq_active[sid] = None
        state.stack.append(-sid-1)
        if not control.assignment.is_true(lit):
            klaus = [lit]
            for pat in state.pat:
                if pat is not None:
                    klaus.append(-pat[0])
            return control.add_clause(klaus) and control.propagate()
        return True

    def __propagate_sequence(self, state, control, sid, lit, items):
        if state.pat_assigned < self.__pat_len and control.assignment.is_false(lit):
            return True
        iid = 0
        for pat in state.pat:
            while True:
                if iid == len(items):
                    return self.__propagate_sequence_lit(state, control, sid, -lit)
                iid+= 1
                if pat is None or items[iid-1] == pat[1]:
                    break
        return state.pat_assigned < self.__pat_len or self.__propagate_sequence_lit(state, control, sid, lit)

    def propagate(self, control, changes):
        state = self.__state(control.thread_id)
        if len(state.trail) == 0 or state.trail[-1][0] < control.assignment.decision_level:
            state.trail.append((control.assignment.decision_level, len(state.stack)))
        for lit in changes:
            for pat in self.__pat_atoms[lit]:
                if state.pat[pat[0]] is not None:
                    # this case should not occur if the pattern is generated properly
                    old = state.pat[pat[0]][0]
                    assert(control.assignment.is_true(old))
                    control.add_clause([-lit, -old]) and control.propagate()
                    return
                else:
                    state.stack.append(pat[0])
                    state.pat_assigned+= 1
                    state.pat[pat[0]] = (lit, pat[1])
                    for sid in self.__occ_list[pat[1]]:
                        if state.seq_active[sid] and not self.__propagate_sequence(state, control, sid, *self.__seq_atoms[sid]):
                            return

    def undo(self, thread_id, assign, undo):
        state = self.__state(thread_id)
        sid = state.trail[-1][1]
        for psid in state.stack[sid:]:
            if psid >= 0:
                state.pat[psid] = None
                state.pat_assigned-= 1
            else:
                state.seq_active[-psid-1] = True
        del state.stack[sid:]
        state.trail.pop()

    # }}}1

def main(prg):
    prg.configuration.solve.opt_mode = "optN"
    prg.register_propagator(Propagator())
    prg.ground([("base", [])])
    with prg.backend() as backend:
        # :- not sup(U), seq(U,_,_), n == 0.
        if prg.get_const("n").number == 0:
            grouped = {}
            for atom in prg.symbolic_atoms.by_signature("seq", 3):
                grouped.setdefault(atom.symbol.arguments[0], []).append(atom.literal)
            for key, value in sorted(grouped.items()):
                a = backend.add_atom()
                for l in value:
                    backend.add_rule([a], [l])
                backend.add_rule([], [-prg.symbolic_atoms[clingo.Function("sup", [key])].literal, a])
        # :- sup(U), pat(_,I), not seq(U,_,I).
        grouped_pat, grouped_seq = {}, set()
        for atom in prg.symbolic_atoms.by_signature("pat", 2):
            grouped_pat.setdefault(atom.symbol.arguments[1], []).append(atom.literal)
        for atom in prg.symbolic_atoms.by_signature("seq", 3):
            grouped_seq.add((atom.symbol.arguments[0], atom.symbol.arguments[2]))
        projected_pat, projected_seq = {}, {}
        for key, value in sorted(grouped_pat.items()):
            a = backend.add_atom()
            for l in value:
                backend.add_rule([a], [l])
            projected_pat[key] = a
        for atom in prg.symbolic_atoms.by_signature("sup", 1):
            for key, lit in sorted(projected_pat.items()):
                if (atom.symbol.arguments[0], key) not in grouped_seq:
                    backend.add_rule([], [atom.literal, lit])
    prg.solve()

#end.

#show pat/2.

#theory seq {
    term { };
    &seq/1 : term, body;
    &pat/0 : term, directive
}.

#const n = 5.

1 { pat(P,I) : seq(_,_,I) } 1 :- P = 0..(n-1).

&pat { P,I : pat(P,I) }.
sup(U) :- &seq(U) { P,I : seq(U,P,I) }, seq(U,_,_).

#maximize { 1,U : sup(U) }.

% abaca
seq(0,0,a).
seq(0,1,b).
seq(0,2,a).
seq(0,3,c).
seq(0,4,a).

% abdca
seq(1,0,a).
seq(1,1,b).
seq(1,2,d).
seq(1,3,c).
seq(1,4,a).

% aedca
seq(2,0,a).
seq(2,1,e).
seq(2,2,d).
seq(2,3,c).
seq(2,4,a).
